#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <ctype.h>

#include "cJSON.h"


#define O_malloc	malloc
#define O_free		free
#define O_realloc	realloc
#define O_strlen	strlen
#define O_strcmp	strcmp
#define O_strncmp	strncmp
#define O_strcpy	strcpy
#define O_strncpy	strncpy
#define O_strchr	strchr
#define O_memset	memset
#define O_memcpy	memcpy
#define O_sprintf	sprintf

#define true		1
#define false		0




static const unsigned char *global_ep = NULL;

const char *cJSON_GetErrorPtr(void)
{
    return (const char*) global_ep;
}


const char*cJSON_Version(void)
{
    static char version[15];
    O_sprintf(version, "%i.%i.%i", CJSON_VERSION_MAJOR, CJSON_VERSION_MINOR, CJSON_VERSION_PATCH);

    return version;
}


/* case insensitive strcmp */
static int cJSON_strcasecmp(const unsigned char *s1, const unsigned char *s2)
{
    if (!s1)
    {
        return (s1 == s2) ? 0 : 1; /* both NULL? */
    }
    if (!s2)
    {
        return 1;
    }
    for(; tolower(*s1) == tolower(*s2); (void)++s1, ++s2)
    {
        if (*s1 == '\0')
        {
            return 0;
        }
    }

    return tolower(*s1) - tolower(*s2);
}


static unsigned char* cJSON_strdup(const unsigned char* str)
{
    size_t len = 0;
    unsigned char *copy = NULL;

    if (str == NULL)
    {
        return NULL;
    }

    len = O_strlen((const char*)str) + 1;
    if (!(copy = (unsigned char*)O_malloc(len)))
    {
        return NULL;
    }
    O_memcpy(copy, str, len);

    return copy;
}


/* Internal constructor. */
static cJSON *cJSON_New_Item()
{
    cJSON* node = (cJSON*)O_malloc(sizeof(cJSON));
    if (node)
    {
        O_memset(node, '\0', sizeof(cJSON));
    }

    return node;
}


/* Delete a cJSON structure. */
void cJSON_Delete(cJSON *c)
{
    cJSON *next = NULL;
    while (c)
    {
        next = c->next;
        if (!(c->type & cJSON_IsReference) && c->child)
        {
            cJSON_Delete(c->child);
        }
        if (!(c->type & cJSON_IsReference) && c->valuestring)
        {
            O_free(c->valuestring);
        }
        if (!(c->type & cJSON_StringIsConst) && c->string)
        {
            O_free(c->string);
        }
        O_free(c);
        c = next;
    }
}


/* Parse the input text to generate a number, and populate the result into item. */
/*
static const unsigned char *parse_number(cJSON * const item, const unsigned char * const input)
{
	char tmp[50];
    int i;
    unsigned char *after_end = NULL;

    if (input == NULL)
    {
        return NULL;
    }

    for(i=0; input[i]; ++i)
    {
    	if(isdigit(input[i]) || (i==0 && input[i]=='-'))
    	{
    		tmp[i] = input[i];
    	}
    	else
    	{
    	    tmp[i]=0;
    		break;
    	}
    }

    after_end = (unsigned char *)input + i;

    if (input == after_end)
    {
    	return NULL; // parse_error
    }

    item->valueint = atoi(tmp);
    item->type = cJSON_Number;

    return after_end;
}
*/

typedef struct
{
    unsigned char *	buffer;
    size_t			length;
    size_t			offset;
    cJSON_bool		noalloc;
} printbuffer;

/* realloc printbuffer if necessary to have at least "needed" bytes more */
static unsigned char* ensure(printbuffer * const p, size_t needed)
{
    unsigned char *newbuffer = NULL;
    size_t newsize = 0;

    if ((p == NULL) || (p->buffer == NULL))
    {
        return NULL;
    }

    if (needed > INT_MAX)
    {
        /* sizes bigger than INT_MAX are currently not supported */
        return NULL;
    }

    needed += p->offset;
    if (needed <= p->length)
    {
        return p->buffer + p->offset;
    }

    if (p->noalloc)
    {
        return NULL;
    }

    /* calculate new buffer size */
    newsize = needed * 2;
    if (newsize > INT_MAX)
    {
        /* overflow of int, use INT_MAX if possible */
        if (needed <= INT_MAX)
        {
            newsize = INT_MAX;
        }
        else
        {
            return NULL;
        }
    }

    newbuffer = (unsigned char*)O_realloc(p->buffer, newsize);

    p->length = newsize;
    p->buffer = newbuffer;

    return newbuffer + p->offset;
}


/* calculate the new length of the string in a printbuffer and update the offset */
static void update_offset(printbuffer * const buffer)
{
    const unsigned char *buffer_pointer = NULL;
    if ((buffer == NULL) || (buffer->buffer == NULL))
    {
        return;
    }
    buffer_pointer = buffer->buffer + buffer->offset;

    buffer->offset += O_strlen((const char*)buffer_pointer);
}


/* Render the number nicely from the given item into a string. */
static cJSON_bool print_number(const cJSON * const item, printbuffer * const output_buffer)
{
    unsigned char *output_pointer = NULL;
    int d = item->valueint;
    int length = 0;

    if (output_buffer == NULL)
    {
        return false;
    }

    /* This is a nice tradeoff. */
    output_pointer = ensure(output_buffer, 64);
    if (output_pointer == NULL)
    {
        return false;
    }

    length = O_sprintf((char*)output_pointer, "%d", d);

    /* sprintf failed */
    if (length < 0)
    {
        return false;
    }

    output_buffer->offset += (size_t)length;

    return true;
}


/* parse 4 digit hexadecimal number */
static unsigned parse_hex4(const unsigned char * const input)
{
    unsigned int h = 0;
    size_t i = 0;

    for (i = 0; i < 4; i++)
    {
        /* parse digit */
        if ((input[i] >= '0') && (input[i] <= '9'))
        {
            h += (unsigned int) input[i] - '0';
        }
        else if ((input[i] >= 'A') && (input[i] <= 'F'))
        {
            h += (unsigned int) 10 + input[i] - 'A';
        }
        else if ((input[i] >= 'a') && (input[i] <= 'f'))
        {
            h += (unsigned int) 10 + input[i] - 'a';
        }
        else /* invalid */
        {
            return 0;
        }

        if (i < 3)
        {
            /* shift left to make place for the next nibble */
            h = h << 4;
        }
    }

    return h;
}


/* converts a UTF-16 literal to UTF-8
 * A literal can be one or two sequences of the form \uXXXX */
static unsigned char utf16_literal_to_utf8(const unsigned char * const input_pointer, const unsigned char * const input_end, unsigned char **output_pointer, const unsigned char **error_pointer)
{
    long unsigned int codepoint = 0;
    unsigned int first_code = 0;
    const unsigned char *first_sequence = input_pointer;
    unsigned char utf8_length = 0;
    unsigned char utf8_position = 0;
    unsigned char sequence_length = 0;
    unsigned char first_byte_mark = 0;

    if ((input_end - first_sequence) < 6)
    {
        /* input ends unexpectedly */
        *error_pointer = first_sequence;
        goto fail;
    }

    /* get the first utf16 sequence */
    first_code = parse_hex4(first_sequence + 2);

    /* check that the code is valid */
    if (((first_code >= 0xDC00) && (first_code <= 0xDFFF)) || (first_code == 0))
    {
        *error_pointer = first_sequence;
        goto fail;
    }

    /* UTF16 surrogate pair */
    if ((first_code >= 0xD800) && (first_code <= 0xDBFF))
    {
        const unsigned char *second_sequence = first_sequence + 6;
        unsigned int second_code = 0;
        sequence_length = 12; /* \uXXXX\uXXXX */

        if ((input_end - second_sequence) < 6)
        {
            /* input ends unexpectedly */
            *error_pointer = first_sequence;
            goto fail;
        }

        if ((second_sequence[0] != '\\') || (second_sequence[1] != 'u'))
        {
            /* missing second half of the surrogate pair */
            *error_pointer = first_sequence;
            goto fail;
        }

        /* get the second utf16 sequence */
        second_code = parse_hex4(second_sequence + 2);
        /* check that the code is valid */
        if ((second_code < 0xDC00) || (second_code > 0xDFFF))
        {
            /* invalid second half of the surrogate pair */
            *error_pointer = first_sequence;
            goto fail;
        }


        /* calculate the unicode codepoint from the surrogate pair */
        codepoint = 0x10000 + (((first_code & 0x3FF) << 10) | (second_code & 0x3FF));
    }
    else
    {
        sequence_length = 6; /* \uXXXX */
        codepoint = first_code;
    }

    /* encode as UTF-8
     * takes at maximum 4 bytes to encode:
     * 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
    if (codepoint < 0x80)
    {
        /* normal ascii, encoding 0xxxxxxx */
        utf8_length = 1;
    }
    else if (codepoint < 0x800)
    {
        /* two bytes, encoding 110xxxxx 10xxxxxx */
        utf8_length = 2;
        first_byte_mark = 0xC0; /* 11000000 */
    }
    else if (codepoint < 0x10000)
    {
        /* three bytes, encoding 1110xxxx 10xxxxxx 10xxxxxx */
        utf8_length = 3;
        first_byte_mark = 0xE0; /* 11100000 */
    }
    else if (codepoint <= 0x10FFFF)
    {
        /* four bytes, encoding 1110xxxx 10xxxxxx 10xxxxxx 10xxxxxx */
        utf8_length = 4;
        first_byte_mark = 0xF0; /* 11110000 */
    }
    else
    {
        /* invalid unicode codepoint */
        *error_pointer = first_sequence;
        goto fail;
    }

    /* encode as utf8 */
    for (utf8_position = (unsigned char)(utf8_length - 1); utf8_position > 0; utf8_position--)
    {
        /* 10xxxxxx */
        (*output_pointer)[utf8_position] = (unsigned char)((codepoint | 0x80) & 0xBF);
        codepoint >>= 6;
    }
    /* encode first byte */
    if (utf8_length > 1)
    {
        (*output_pointer)[0] = (unsigned char)((codepoint | first_byte_mark) & 0xFF);
    }
    else
    {
        (*output_pointer)[0] = (unsigned char)(codepoint & 0x7F);
    }

    *output_pointer += utf8_length;

    return sequence_length;

fail:
    return 0;
}


/* Parse the input text into an unescaped cinput, and populate item. */
static const unsigned char *parse_string(cJSON * const item, const unsigned char * const input, const unsigned char ** const error_pointer)
{
    const unsigned char *input_pointer = input + 1;
    const unsigned char *input_end = input + 1;
    unsigned char *output_pointer = NULL;
    unsigned char *output = NULL;

    /* not a string */
    if (*input != '\"')
    {
        *error_pointer = input;
        goto fail;
    }

    {
        /* calculate approximate size of the output (overestimate) */
        size_t allocation_length = 0;
        size_t skipped_bytes = 0;
        while ((*input_end != '\"') && (*input_end != '\0'))
        {
            /* is escape sequence */
            if (input_end[0] == '\\')
            {
                if (input_end[1] == '\0')
                {
                    /* prevent buffer overflow when last input character is a backslash */
                    goto fail;
                }
                skipped_bytes++;
                input_end++;
            }
            input_end++;
        }
        if (*input_end == '\0')
        {
            goto fail; /* string ended unexpectedly */
        }

        /* This is at most how much we need for the output */
        allocation_length = (size_t) (input_end - input) - skipped_bytes;
        output = (unsigned char*)O_malloc(allocation_length + sizeof('\0'));
        if (output == NULL)
        {
            goto fail; /* allocation failure */
        }
    }

    output_pointer = output;
    /* loop through the string literal */
    while (input_pointer < input_end)
    {
        if (*input_pointer != '\\')
        {
            *output_pointer++ = *input_pointer++;
        }
        /* escape sequence */
        else
        {
            unsigned char sequence_length = 2;
            switch (input_pointer[1])
            {
                case 'b':
                    *output_pointer++ = '\b';
                    break;
                case 'f':
                    *output_pointer++ = '\f';
                    break;
                case 'n':
                    *output_pointer++ = '\n';
                    break;
                case 'r':
                    *output_pointer++ = '\r';
                    break;
                case 't':
                    *output_pointer++ = '\t';
                    break;
                case '\"':
                case '\\':
                case '/':
                    *output_pointer++ = input_pointer[1];
                    break;

                /* UTF-16 literal */
                case 'u':
                    sequence_length = utf16_literal_to_utf8(input_pointer, input_end, &output_pointer, error_pointer);
                    if (sequence_length == 0)
                    {
                        /* failed to convert UTF16-literal to UTF-8 */
                        goto fail;
                    }
                    break;

                default:
                    *error_pointer = input_pointer;
                    goto fail;
            }
            input_pointer += sequence_length;
        }
    }

    /* zero terminate the output */
    *output_pointer = '\0';

    item->type = cJSON_String;
    item->valuestring = (char*)output;

    return input_end + 1;

fail:
    if (output != NULL)
    {
        O_free(output);
    }

    return NULL;
}



static const unsigned char *parse_number_as_string(cJSON * const item, const unsigned char * const input)
{
    const unsigned char *input_pointer = input;
    const unsigned char *input_end = input;
    unsigned char *output_pointer = NULL;
    unsigned char *output = NULL;
	size_t allocation_length = 0;
	int dotcount = 0;

	while (	(isdigit(*input_end)) || (*input_end == '.') || (*input_end == '-') )
	{
		if(*input_end == '.')
		{
			if(dotcount) return NULL;
			dotcount = 1;
		}
		input_end++;
	}

	if (*input_end == '\0')	return NULL; // number ended unexpectedly

	allocation_length = (size_t) (input_end - input);
	output = (unsigned char*)O_malloc(allocation_length + sizeof('\0'));

	if (output == NULL) return NULL; // allocation failure

    output_pointer = output;
    while (input_pointer < input_end) *output_pointer++ = *input_pointer++;
    *output_pointer = '\0';

    if(dotcount)
	{
		item->type = cJSON_String;
		item->valuestring = (char*)output;
	}
	else
	{
		item->type = cJSON_Number;
		item->valueint = atoi((const char*)output);
	}

    return input_end;
}


/* Render the cstring provided to an escaped version that can be printed. */
static cJSON_bool print_string_ptr(const unsigned char * const input, printbuffer * const output_buffer)
{
    const unsigned char *input_pointer = NULL;
    unsigned char *output = NULL;
    unsigned char *output_pointer = NULL;
    size_t output_length = 0;
    /* numbers of additional characters needed for escaping */
    size_t escape_characters = 0;

    if (output_buffer == NULL)
    {
        return false;
    }

    /* empty string */
    if (input == NULL)
    {
        output = ensure(output_buffer, sizeof("\"\""));
        if (output == NULL)
        {
            return false;
        }
        O_strcpy((char*)output, "\"\"");

        return true;
    }

    /* set "flag" to 1 if something needs to be escaped */
    for (input_pointer = input; *input_pointer; input_pointer++)
    {
        if (O_strchr("\"\\\b\f\n\r\t", *input_pointer))
        {
            /* one character escape sequence */
            escape_characters++;
        }
        else if (*input_pointer < 32)
        {
            /* UTF-16 escape sequence uXXXX */
            escape_characters += 5;
        }
    }
    output_length = (size_t)(input_pointer - input) + escape_characters;

    output = ensure(output_buffer, output_length + sizeof("\"\""));
    if (output == NULL)
    {
        return false;
    }

    /* no characters have to be escaped */
    if (escape_characters == 0)
    {
        O_memcpy(output, input, output_length);
        output[output_length] = '\0';

        return true;
    }

    output_pointer = output;
    /* copy the string */
    for (input_pointer = input; *input_pointer != '\0'; (void)input_pointer++, output_pointer++)
    {
        if ((*input_pointer > 31) && (*input_pointer != '\"') && (*input_pointer != '\\'))
        {
            /* normal character, copy */
            *output_pointer = *input_pointer;
        }
        else
        {
            /* character needs to be escaped */
            *output_pointer++ = '\\';
            switch (*input_pointer)
            {
                case '\\':
                    *output_pointer = '\\';
                    break;
                case '\"':
                    *output_pointer = '\"';
                    break;
                case '\b':
                    *output_pointer = 'b';
                    break;
                case '\f':
                    *output_pointer = 'f';
                    break;
                case '\n':
                    *output_pointer = 'n';
                    break;
                case '\r':
                    *output_pointer = 'r';
                    break;
                case '\t':
                    *output_pointer = 't';
                    break;
                default:
                    /* escape and print as unicode codepoint */
                    O_sprintf((char*)output_pointer, "u%04x", *input_pointer);
                    output_pointer += 4;
                    break;
            }
        }
    }
    output[output_length] = '\0';

    return true;
}


/* Invoke print_string_ptr (which is useful) on an item. */
static cJSON_bool print_string(const cJSON * const item, printbuffer * const p)
{
    return print_string_ptr((unsigned char*)item->valuestring, p);
}


/* Predeclare these prototypes. */
static const unsigned char *parse_value(cJSON * const item, const unsigned char * const input, const unsigned char ** const ep);
static cJSON_bool print_value(const cJSON * const item, const size_t depth, const cJSON_bool format, printbuffer * const output_buffer);
static const unsigned char *parse_array(cJSON * const item, const unsigned char *input, const unsigned char ** const ep);
static cJSON_bool print_array(const cJSON * const item, const size_t depth, const cJSON_bool format, printbuffer * const output_buffer);
static const unsigned char *parse_object(cJSON * const item, const unsigned char *input, const unsigned char ** const ep);
static cJSON_bool print_object(const cJSON * const item, const size_t depth, const cJSON_bool format, printbuffer * const output_buffer);


/* Utility to jump whitespace and cr/lf */
static const unsigned char *skip_whitespace(const unsigned char *in)
{
    while (in && *in && (*in <= 32))
    {
        in++;
    }

    return in;
}


/* Parse an object - create a new root, and populate. */
cJSON *cJSON_ParseWithOpts(const char *value, const char **return_parse_end, cJSON_bool require_null_terminated)
{
    const unsigned char *end = NULL;
    /* use global error pointer if no specific one was given */
    const unsigned char **ep = return_parse_end ? (const unsigned char**)return_parse_end : &global_ep;
    cJSON *c = cJSON_New_Item();
    *ep = NULL;
    if (!c) /* memory fail */
    {
        return NULL;
    }

    end = parse_value(c, skip_whitespace((const unsigned char*)value), ep);
    if (!end)
    {
        /* parse failure. ep is set. */
        cJSON_Delete(c);
        return NULL;
    }

    /* if we require null-terminated JSON without appended garbage, skip and then check for a null terminator */
    if (require_null_terminated)
    {
        end = skip_whitespace(end);
        if (*end)
        {
            cJSON_Delete(c);
            *ep = end;
            return NULL;
        }
    }
    if (return_parse_end)
    {
        *return_parse_end = (const char*)end;
    }

    return c;
}


/* Default options for cJSON_Parse */
cJSON *cJSON_Parse(const char *value)
{
    return cJSON_ParseWithOpts(value, 0, 0);
}


#define min(a, b) ((a < b) ? a : b)

static unsigned char *print(const cJSON * const item, cJSON_bool format)
{
    printbuffer buffer[1];
    unsigned char *printed = NULL;

    O_memset(buffer, 0, sizeof(buffer));

    /* create buffer */
    buffer->buffer = (unsigned char*)O_malloc(256);
    if (buffer->buffer == NULL)
    {
        goto fail;
    }

    /* print the value */
    if (!print_value(item, 0, format, buffer))
    {
        goto fail;
    }
    update_offset(buffer);

    /* copy the buffer over to a new one */
    printed = (unsigned char*)O_malloc(buffer->offset + 1);
    if (printed == NULL)
    {
        goto fail;
    }
    O_strncpy((char*)printed, (char*)buffer->buffer, min(buffer->length, buffer->offset + 1));
    printed[buffer->offset] = '\0'; /* just to be sure */

    /* free the buffer */
    O_free(buffer->buffer);

    return printed;

fail:
    if (buffer->buffer != NULL)
    {
        O_free(buffer->buffer);
    }

    if (printed != NULL)
    {
        O_free(printed);
    }

    return NULL;
}


/* Render a cJSON item/entity/structure to text. */
char *cJSON_Print(const cJSON *item)
{
    return (char*)print(item, true);
}


char *cJSON_PrintUnformatted(const cJSON *item)
{
    return (char*)print(item, false);
}


char *cJSON_PrintBuffered(const cJSON *item, int prebuffer, cJSON_bool fmt)
{
    printbuffer p;

    if (prebuffer < 0)
    {
        return NULL;
    }

    p.buffer = (unsigned char*)O_malloc((size_t)prebuffer);
    if (!p.buffer)
    {
        return NULL;
    }

    p.length = (size_t)prebuffer;
    p.offset = 0;
    p.noalloc = false;

    if (!print_value(item, 0, fmt, &p))
    {
        return NULL;
    }

    return (char*)p.buffer;
}


cJSON_bool cJSON_PrintPreallocated(cJSON *item, char *buf, const int len, const cJSON_bool fmt)
{
    printbuffer p;

    if (len < 0)
    {
        return false;
    }

    p.buffer = (unsigned char*)buf;
    p.length = (size_t)len;
    p.offset = 0;
    p.noalloc = true;
    return print_value(item, 0, fmt, &p);
}


/* Parser core - when encountering text, process appropriately. */
static const unsigned  char *parse_value(cJSON * const item, const unsigned char * const input, const unsigned char ** const error_pointer)
{
    if (input == NULL)
    {
        return NULL; /* no input */
    }

    /* parse the different types of values */
    /* null */
    if (!O_strncmp((const char*)input, "null", 4))
    {
        item->type = cJSON_NULL;
        return input + 4;
    }
    /* false */
    if (!O_strncmp((const char*)input, "false", 5))
    {
        item->type = cJSON_False;
        return input + 5;
    }
    /* true */
    if (!O_strncmp((const char*)input, "true", 4))
    {
        item->type = cJSON_True;
        item->valueint = 1;
        return input + 4;
    }
    /* string */
    if (*input == '\"')
    {
        return parse_string(item, input, error_pointer);
    }
    /* number */
    if ((*input == '-') || ((*input >= '0') && (*input <= '9')))
    {
    	return parse_number_as_string(item, input);
        //return parse_number(item, input);
    }
    /* array */
    if (*input == '[')
    {
        return parse_array(item, input, error_pointer);
    }
    /* object */
    if (*input == '{')
    {
        return parse_object(item, input, error_pointer);
    }

    /* failure. */
    *error_pointer = input;
    return NULL;
}


/* Render a value to text. */
static cJSON_bool print_value(const cJSON * const item, const size_t depth, const cJSON_bool format,  printbuffer * const output_buffer)
{
    unsigned char *output = NULL;

    if ((item == NULL) || (output_buffer == NULL))
    {
        return false;
    }

    switch ((item->type) & 0xFF)
    {
        case cJSON_NULL:
            output = ensure(output_buffer, 5);
            if (output == NULL)
            {
                return false;
            }
            O_strcpy((char*)output, "null");
            return true;

        case cJSON_False:
            output = ensure(output_buffer, 6);
            if (output == NULL)
            {
                return false;
            }
            O_strcpy((char*)output, "false");
            return true;

        case cJSON_True:
            output = ensure(output_buffer, 5);
            if (output == NULL)
            {
                return false;
            }
            O_strcpy((char*)output, "true");
            return true;

		case cJSON_Number:
			return print_number(item, output_buffer);

        case cJSON_Raw:
        {
            size_t raw_length = 0;
            if (item->valuestring == NULL)
            {
                if (!output_buffer->noalloc)
                {
                    O_free(output_buffer->buffer);
                }
                return false;
            }

            raw_length = O_strlen(item->valuestring) + sizeof('\0');
            output = ensure(output_buffer, raw_length);
            if (output == NULL)
            {
                return false;
            }
            O_memcpy(output, item->valuestring, raw_length);
            return true;
        }

        case cJSON_String:
            return print_string(item, output_buffer);

        case cJSON_Array:
            return print_array(item, depth, format, output_buffer);

        case cJSON_Object:
            return print_object(item, depth, format, output_buffer);

        default:
            return false;
    }
}


/* Build an array from input text. */
static const unsigned char *parse_array(cJSON * const item, const unsigned char *input, const unsigned char ** const error_pointer)
{
    cJSON *head = NULL; /* head of the linked list */
    cJSON *current_item = NULL;

    if (*input != '[')
    {
        /* not an array */
        *error_pointer = input;
        goto fail;
    }

    input = skip_whitespace(input + 1);
    if (*input == ']')
    {
        /* empty array */
        goto success;
    }

    /* step back to character in front of the first element */
    input--;

    /* loop through the comma separated array elements */
    do
    {
        /* allocate next item */
        cJSON *new_item = cJSON_New_Item();
        if (new_item == NULL)
        {
            goto fail; /* allocation failure */
        }

        /* attach next item to list */
        if (head == NULL)
        {
            /* start the linked list */
            current_item = head = new_item;
        }
        else
        {
            /* add to the end and advance */
            current_item->next = new_item;
            new_item->prev = current_item;
            current_item = new_item;
        }

        /* parse next value */
        input = skip_whitespace(input + 1);
        input = parse_value(current_item, input, error_pointer);
        input = skip_whitespace(input);
        if (input == NULL)
        {
            goto fail; /* failed to parse value */
        }
    }
    while (*input == ',');

    if (*input != ']')
    {
        *error_pointer = input;
        goto fail; /* expected end of array */
    }

success:
    item->type = cJSON_Array;
    item->child = head;

    return input + 1;

fail:
    if (head != NULL)
    {
        cJSON_Delete(head);
    }

    return NULL;
}


/* Render an array to text */
static cJSON_bool print_array(const cJSON * const item, const size_t depth, const cJSON_bool format, printbuffer * const output_buffer)
{
    unsigned char *output_pointer = NULL;
    size_t length = 0;
    cJSON *current_element = item->child;

    if (output_buffer == NULL)
    {
        return false;
    }

    /* Compose the output array. */
    /* opening square bracket */
    output_pointer = ensure(output_buffer, 1);
    if (output_pointer == NULL)
    {
        return false;
    }

    *output_pointer = '[';
    output_buffer->offset++;

    while (current_element != NULL)
    {
        if (!print_value(current_element, depth + 1, format, output_buffer))
        {
            return false;
        }
        update_offset(output_buffer);
        if (current_element->next)
        {
            length = format ? 2 : 1;
            output_pointer = ensure(output_buffer, length + 1);
            if (output_pointer == NULL)
            {
                return false;
            }
            *output_pointer++ = ',';
            if(format)
            {
                *output_pointer++ = ' ';
            }
            *output_pointer = '\0';
            output_buffer->offset += length;
        }
        current_element = current_element->next;
    }

    output_pointer = ensure(output_buffer, 2);
    if (output_pointer == NULL)
    {
        return false;
    }
    *output_pointer++ = ']';
    *output_pointer = '\0';

    return true;
}


/* Build an object from the text. */
static const unsigned char *parse_object(cJSON * const item, const unsigned char *input, const unsigned char ** const error_pointer)
{
    cJSON *head = NULL; /* linked list head */
    cJSON *current_item = NULL;

    if (*input != '{')
    {
        *error_pointer = input;
        goto fail; /* not an object */
    }

    input = skip_whitespace(input + 1);
    if (*input == '}')
    {
        goto success; /* empty object */
    }

    /* step back to character in front of the first element */
    input--;

    /* loop through the comma separated array elements */
    do
    {
        /* allocate next item */
        cJSON *new_item = cJSON_New_Item();
        if (new_item == NULL)
        {
            goto fail; /* allocation failure */
        }

        /* attach next item to list */
        if (head == NULL)
        {
            /* start the linked list */
            current_item = head = new_item;
        }
        else
        {
            /* add to the end and advance */
            current_item->next = new_item;
            new_item->prev = current_item;
            current_item = new_item;
        }

        /* parse the name of the child */
        input = skip_whitespace(input + 1);
        input = parse_string(current_item, input, error_pointer);
        input = skip_whitespace(input);
        if (input == NULL)
        {
            goto fail; /* faile to parse name */
        }

        /* swap valuestring and string, because we parsed the name */
        current_item->string = current_item->valuestring;
        current_item->valuestring = NULL;

        if (*input != ':')
        {
            *error_pointer = input;
            goto fail; /* invalid object */
        }

        /* parse the value */
        input = skip_whitespace(input + 1);
        input = parse_value(current_item, input, error_pointer);
        input = skip_whitespace(input);
        if (input == NULL)
        {
            goto fail; /* failed to parse value */
        }
    }
    while (*input == ',');

    if (*input != '}')
    {
        *error_pointer = input;
        goto fail; /* expected end of object */
    }

success:
    item->type = cJSON_Object;
    item->child = head;

    return input + 1;

fail:
    if (head != NULL)
    {
        cJSON_Delete(head);
    }

    return NULL;
}


/* Render an object to text. */
static cJSON_bool print_object(const cJSON * const item, const size_t depth, const cJSON_bool format, printbuffer * const output_buffer)
{
    unsigned char *output_pointer = NULL;
    size_t length = 0;
    cJSON *current_item = item->child;

    if (output_buffer == NULL)
    {
        return false;
    }

    /* Compose the output: */
    length = format ? 2 : 1; /* fmt: {\n */
    output_pointer = ensure(output_buffer, length + 1);
    if (output_pointer == NULL)
    {
        return false;
    }

    *output_pointer++ = '{';
    if (format)
    {
        *output_pointer++ = '\n';
    }
    output_buffer->offset += length;

    while (current_item)
    {
        if (format)
        {
            size_t i;
            output_pointer = ensure(output_buffer, depth + 1);
            if (output_pointer == NULL)
            {
                return false;
            }
            for (i = 0; i < depth + 1; i++)
            {
                *output_pointer++ = '\t';
            }
            output_buffer->offset += depth + 1;
        }

        /* print key */
        if (!print_string_ptr((unsigned char*)current_item->string, output_buffer))
        {
            return false;
        }
        update_offset(output_buffer);

        length = format ? 2 : 1;
        output_pointer = ensure(output_buffer, length);
        if (output_pointer == NULL)
        {
            return false;
        }
        *output_pointer++ = ':';
        if (format)
        {
            *output_pointer++ = '\t';
        }
        output_buffer->offset += length;

        /* print value */
        if (!print_value(current_item, depth + 1, format, output_buffer))
        {
            return false;
        }
        update_offset(output_buffer);

        /* print comma if not last */
        length = (size_t) (format ? 1 : 0) + (current_item->next ? 1 : 0);
        output_pointer = ensure(output_buffer, length + 1);
        if (output_pointer == NULL)
        {
            return false;
        }
        if (current_item->next)
        {
            *output_pointer++ = ',';
        }

        if (format)
        {
            *output_pointer++ = '\n';
        }
        *output_pointer = '\0';
        output_buffer->offset += length;

        current_item = current_item->next;
    }

    output_pointer = ensure(output_buffer, format ? (depth + 2) : 2);
    if (output_pointer == NULL)
    {
        return false;
    }
    if (format)
    {
        size_t i;
        for (i = 0; i < (depth); i++)
        {
            *output_pointer++ = '\t';
        }
    }
    *output_pointer++ = '}';
    *output_pointer = '\0';

    return true;
}


/* Get Array size/item / object item. */
int cJSON_GetArraySize(const cJSON *array)
{
    cJSON *c = array->child;
    size_t i = 0;
    while(c)
    {
        i++;
        c = c->next;
    }

    /* FIXME: Can overflow here. Cannot be fixed without breaking the API */

    return (int)i;
}


cJSON *cJSON_GetArrayItem(const cJSON *array, int item)
{
    cJSON *c = array ? array->child : NULL;
    while (c && item > 0)
    {
        item--;
        c = c->next;
    }

    return c;
}


cJSON *cJSON_GetObjectItem(const cJSON *object, const char *string)
{
    cJSON *c = object ? object->child : NULL;
    while (c && cJSON_strcasecmp((unsigned char*)c->string, (const unsigned char*)string))
    {
        c = c->next;
    }
    return c;
}


cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON * const object, const char * const string)
{
    cJSON *current_element = NULL;

    if ((object == NULL) || (string == NULL))
    {
        return NULL;
    }

    current_element = object->child;
    while ((current_element != NULL) && (O_strcmp(string, current_element->string) != 0))
    {
        current_element = current_element->next;
    }

    return current_element;
}


cJSON_bool cJSON_HasObjectItem(const cJSON *object, const char *string)
{
    return cJSON_GetObjectItem(object, string) ? 1 : 0;
}


/* Utility for array list handling. */
static void suffix_object(cJSON *prev, cJSON *item)
{
    prev->next = item;
    item->prev = prev;
}


/* Utility for handling references. */
static cJSON *create_reference(const cJSON *item)
{
    cJSON *ref = cJSON_New_Item();
    if (!ref)
    {
        return NULL;
    }

    O_memcpy(ref, item, sizeof(cJSON));

    ref->string = NULL;
    ref->type |= cJSON_IsReference;
    ref->next = ref->prev = NULL;

    return ref;
}


/* Add item to array/object. */
void cJSON_AddItemToArray(cJSON *array, cJSON *item)
{
    cJSON *child = NULL;

    if ((item == NULL) || (array == NULL))
    {
        return;
    }

    child = array->child;

    if (child == NULL)
    {
        /* list is empty, start new one */
        array->child = item;
    }
    else
    {
        /* append to the end */
        while (child->next)
        {
            child = child->next;
        }
        suffix_object(child, item);
    }
}


void cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item)
{
    /* call cJSON_AddItemToObjectCS for code reuse */
    cJSON_AddItemToObjectCS(object, (char*)cJSON_strdup((const unsigned char*)string), item);

    /* remove cJSON_StringIsConst flag */
    item->type &= ~cJSON_StringIsConst;
}


/* Add an item to an object with constant string as key */
void cJSON_AddItemToObjectCS(cJSON *object, const char *string, cJSON *item)
{
    if (!item)
    {
        return;
    }

    if (!(item->type & cJSON_StringIsConst) && item->string)
    {
        O_free(item->string);
    }

    item->string = (char*)string;
    item->type |= cJSON_StringIsConst;

    cJSON_AddItemToArray(object, item);
}


void cJSON_AddItemReferenceToArray(cJSON *array, cJSON *item)
{
    cJSON_AddItemToArray(array, create_reference(item));
}


void cJSON_AddItemReferenceToObject(cJSON *object, const char *string, cJSON *item)
{
    cJSON_AddItemToObject(object, string, create_reference(item));
}


static cJSON *DetachItemFromArray(cJSON *array, size_t which)
{
    cJSON *c = array->child;
    while (c && (which > 0))
    {
        c = c->next;
        which--;
    }
    if (!c)
    {
        /* item doesn't exist */
        return NULL;
    }
    if (c->prev)
    {
        /* not the first element */
        c->prev->next = c->next;
    }
    if (c->next)
    {
        c->next->prev = c->prev;
    }
    if (c==array->child)
    {
        array->child = c->next;
    }
    /* make sure the detached item doesn't point anywhere anymore */
    c->prev = c->next = NULL;

    return c;
}
cJSON *cJSON_DetachItemFromArray(cJSON *array, int which)
{
    if (which < 0)
    {
        return NULL;
    }

    return DetachItemFromArray(array, (size_t)which);
}


void cJSON_DeleteItemFromArray(cJSON *array, int which)
{
    cJSON_Delete(cJSON_DetachItemFromArray(array, which));
}


cJSON *cJSON_DetachItemFromObject(cJSON *object, const char *string)
{
    size_t i = 0;
    cJSON *c = object->child;

    while (c && cJSON_strcasecmp((unsigned char*)c->string, (const unsigned char*)string))
    {
        i++;
        c = c->next;
    }

    if (c)
    {
        return DetachItemFromArray(object, i);
    }

    return NULL;
}


void cJSON_DeleteItemFromObject(cJSON *object, const char *string)
{
    cJSON_Delete(cJSON_DetachItemFromObject(object, string));
}


/* Replace array/object items with new ones. */
void cJSON_InsertItemInArray(cJSON *array, int which, cJSON *newitem)
{
    cJSON *c = array->child;

    while (c && (which > 0))
    {
        c = c->next;
        which--;
    }

    if (!c)
    {
        cJSON_AddItemToArray(array, newitem);
        return;
    }

    newitem->next = c;
    newitem->prev = c->prev;

    c->prev = newitem;

    if (c == array->child)
    {
        array->child = newitem;
    }

    else
    {
        newitem->prev->next = newitem;
    }
}


static void ReplaceItemInArray(cJSON *array, size_t which, cJSON *newitem)
{
    cJSON *c = array->child;

    while (c && (which > 0))
    {
        c = c->next;
        which--;
    }

    if (!c)
    {
        return;
    }

    newitem->next = c->next;
    newitem->prev = c->prev;

    if (newitem->next)
    {
        newitem->next->prev = newitem;
    }

    if (c == array->child)
    {
        array->child = newitem;
    }
    else
    {
        newitem->prev->next = newitem;
    }

    c->next = c->prev = NULL;

    cJSON_Delete(c);
}
void cJSON_ReplaceItemInArray(cJSON *array, int which, cJSON *newitem)
{
    if (which < 0)
    {
        return;
    }

    ReplaceItemInArray(array, (size_t)which, newitem);
}


void cJSON_ReplaceItemInObject(cJSON *object, const char *string, cJSON *newitem)
{
    size_t i = 0;
    cJSON *c = object->child;

    while(c && cJSON_strcasecmp((unsigned char*)c->string, (const unsigned char*)string))
    {
        i++;
        c = c->next;
    }

    if(c)
    {
        /* free the old string if not const */
        if (!(newitem->type & cJSON_StringIsConst) && newitem->string)
        {
             O_free(newitem->string);
        }

        newitem->string = (char*)cJSON_strdup((const unsigned char*)string);
        ReplaceItemInArray(object, i, newitem);
    }
}


/* Create basic types: */
cJSON *cJSON_CreateNull(void)
{
    cJSON *item = cJSON_New_Item();

    if(item)
    {
        item->type = cJSON_NULL;
    }

    return item;
}


cJSON *cJSON_CreateTrue(void)
{
    cJSON *item = cJSON_New_Item();

    if(item)
    {
        item->type = cJSON_True;
    }

    return item;
}


cJSON *cJSON_CreateFalse(void)
{
    cJSON *item = cJSON_New_Item();

    if(item)
    {
        item->type = cJSON_False;
    }

    return item;
}


cJSON *cJSON_CreateBool(cJSON_bool b)
{
    cJSON *item = cJSON_New_Item();

    if(item)
    {
        item->type = b ? cJSON_True : cJSON_False;
    }

    return item;
}


cJSON *cJSON_CreateNumber(int num)
{
    cJSON *item = cJSON_New_Item();

    if(item)
    {
        item->type = cJSON_Number;
        item->valueint = num;
    }

    return item;
}


cJSON *cJSON_CreateString(const char *string)
{
    cJSON *item = cJSON_New_Item();

    if(item)
    {
        item->type = cJSON_String;
        item->valuestring = (char*)cJSON_strdup((const unsigned char*)string);

        if(!item->valuestring)
        {
            cJSON_Delete(item);
            return NULL;
        }
    }

    return item;
}


cJSON *cJSON_CreateRaw(const char *raw)
{
    cJSON *item = cJSON_New_Item();

    if(item)
    {
        item->type = cJSON_Raw;
        item->valuestring = (char*)cJSON_strdup((const unsigned char*)raw);

        if(!item->valuestring)
        {
            cJSON_Delete(item);
            return NULL;
        }
    }

    return item;
}


cJSON *cJSON_CreateArray(void)
{
    cJSON *item = cJSON_New_Item();

    if(item)
    {
        item->type=cJSON_Array;
    }

    return item;
}


cJSON *cJSON_CreateObject(void)
{
    cJSON *item = cJSON_New_Item();

    if (item)
    {
        item->type = cJSON_Object;
    }

    return item;
}


/* Create Arrays: */
cJSON *cJSON_CreateIntArray(const int *numbers, int count)
{
    size_t i = 0;
    cJSON *n = NULL;
    cJSON *p = NULL;
    cJSON *a = NULL;

    if (count < 0)
    {
        return NULL;
    }

    a = cJSON_CreateArray();
    for(i = 0; a && (i < (size_t)count); i++)
    {
        n = cJSON_CreateNumber(numbers[i]);
        if (!n)
        {
            cJSON_Delete(a);
            return NULL;
        }
        if(!i)
        {
            a->child = n;
        }
        else
        {
            suffix_object(p, n);
        }
        p = n;
    }

    return a;
}


cJSON *cJSON_CreateStringArray(const char **strings, int count)
{
    size_t i = 0;
    cJSON *n = NULL;
    cJSON *p = NULL;
    cJSON *a = NULL;

    if (count < 0)
    {
        return NULL;
    }

    a = cJSON_CreateArray();

    for (i = 0; a && (i < (size_t)count); i++)
    {
        n = cJSON_CreateString(strings[i]);
        if(!n)
        {
            cJSON_Delete(a);
            return NULL;
        }
        if(!i)
        {
            a->child = n;
        }
        else
        {
            suffix_object(p,n);
        }
        p = n;
    }

    return a;
}


/* Duplication */
cJSON *cJSON_Duplicate(const cJSON *item, cJSON_bool recurse)
{
    cJSON *newitem = NULL;
    cJSON *child = NULL;
    cJSON *next = NULL;
    cJSON *newchild = NULL;

    /* Bail on bad ptr */
    if (!item)
    {
        goto fail;
    }
    /* Create new item */
    newitem = cJSON_New_Item();
    if (!newitem)
    {
        goto fail;
    }
    /* Copy over all vars */
    newitem->type = item->type & (~cJSON_IsReference);
    newitem->valueint = item->valueint;

    if (item->valuestring)
    {
        newitem->valuestring = (char*)cJSON_strdup((unsigned char*)item->valuestring);
        if (!newitem->valuestring)
        {
            goto fail;
        }
    }
    if (item->string)
    {
        newitem->string = (item->type&cJSON_StringIsConst) ? item->string : (char*)cJSON_strdup((unsigned char*)item->string);
        if (!newitem->string)
        {
            goto fail;
        }
    }
    /* If non-recursive, then we're done! */
    if (!recurse)
    {
        return newitem;
    }
    /* Walk the ->next chain for the child. */
    child = item->child;
    while (child != NULL)
    {
        newchild = cJSON_Duplicate(child, true); /* Duplicate (with recurse) each item in the ->next chain */
        if (!newchild)
        {
            goto fail;
        }
        if (next != NULL)
        {
            /* If newitem->child already set, then crosswire ->prev and ->next and move on */
            next->next = newchild;
            newchild->prev = next;
            next = newchild;
        }
        else
        {
            /* Set newitem->child and move to it */
            newitem->child = newchild;
            next = newchild;
        }
        child = child->next;
    }

    return newitem;

fail:
    if (newitem != NULL)
    {
        cJSON_Delete(newitem);
    }

    return NULL;
}


void cJSON_Minify(char *json)
{
    unsigned char *into = (unsigned char*)json;
    while (*json)
    {
        if (*json == ' ')
        {
            json++;
        }
        else if (*json == '\t')
        {
            /* Whitespace characters. */
            json++;
        }
        else if (*json == '\r')
        {
            json++;
        }
        else if (*json=='\n')
        {
            json++;
        }
        else if ((*json == '/') && (json[1] == '/'))
        {
            /* double-slash comments, to end of line. */
            while (*json && (*json != '\n'))
            {
                json++;
            }
        }
        else if ((*json == '/') && (json[1] == '*'))
        {
            /* multiline comments. */
            while (*json && !((*json == '*') && (json[1] == '/')))
            {
                json++;
            }
            json += 2;
        }
        else if (*json == '\"')
        {
            /* string literals, which are \" sensitive. */
            *into++ = (unsigned char)*json++;
            while (*json && (*json != '\"'))
            {
                if (*json == '\\')
                {
                    *into++ = (unsigned char)*json++;
                }
                *into++ = (unsigned char)*json++;
            }
            *into++ = (unsigned char)*json++;
        }
        else
        {
            /* All other characters. */
            *into++ = (unsigned char)*json++;
        }
    }

    /* and null-terminate. */
    *into = '\0';
}


cJSON_bool cJSON_IsInvalid(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_Invalid;
}


cJSON_bool cJSON_IsFalse(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_False;
}


cJSON_bool cJSON_IsTrue(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xff) == cJSON_True;
}


cJSON_bool cJSON_IsBool(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & (cJSON_True | cJSON_False)) != 0;
}
cJSON_bool cJSON_IsNull(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_NULL;
}


cJSON_bool cJSON_IsNumber(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_Number;
}


cJSON_bool cJSON_IsString(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_String;
}


cJSON_bool cJSON_IsArray(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_Array;
}


cJSON_bool cJSON_IsObject(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_Object;
}


cJSON_bool cJSON_IsRaw(const cJSON * const item)
{
    if (item == NULL)
    {
        return false;
    }

    return (item->type & 0xFF) == cJSON_Raw;
}






/* JSON Pointer implementation: */
static int cJSON_Pstrcasecmp(const unsigned char *a, const unsigned char *e)
{
    if (!a || !e)
    {
        return (a == e) ? 0 : 1; /* both NULL? */
    }
    for (; *a && *e && ((*e != '/') && (*e != '[')); (void)a++, e++) /* compare until next '/' */
    {
        if (*e == '~')
        {
            /* check for escaped '~' (~0) and '/' (~1) */
            if (!((e[1] == '0') && (*a == '~')) && !((e[1] == '1') && (*a == '/')))
            {
                /* invalid escape sequence or wrong character in *a */
                return 1;
            }
            else
            {
                e++;
            }
        }
        else if (tolower(*a) != tolower(*e))
        {
            return 1;
        }
    }
    if (((*e != 0) && (*e != '/') && (*e != '[')) != (*a != 0))
    {
        /* one string has ended, the other not */
        return 1;
    }

    return 0;
}


cJSON *cJSON_GetPointer(cJSON *object, const char *pointer)
{
    /* follow path of the pointer */
    while ( object && ( *pointer == '[' || *pointer == '/') )
    {
    	pointer++;

        if (cJSON_IsArray(object))
        {
            size_t which = 0;
            /* parse array index */
            while ((*pointer >= '0') && (*pointer <= '9'))
            {
                which = (10 * which) + (size_t)(*pointer++ - '0');
            }
            if (*pointer++ != ']')
			{
				return NULL;
			}
            if (*pointer && (*pointer != '/'))
            {
                /* not end of string or new path token */
                return NULL;
            }

            object = cJSON_GetArrayItem(object, (int)which);
        }
        else if (cJSON_IsObject(object))
        {
            object = object->child;
            /* GetObjectItem. */
            while (object && cJSON_Pstrcasecmp((unsigned char*)object->string, (const unsigned char*)pointer))
            {
                object = object->next;
            }
            /* skip to the next path token or end of string */
            while (*pointer && (*pointer != '/') && (*pointer != '['))
            {
                pointer++;
            }
        }
        else
        {
            return NULL;
        }
    }

    return object;
}

