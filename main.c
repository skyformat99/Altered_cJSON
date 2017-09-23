#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "cJSON_Utils.h"

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


typedef enum {stSign, stInt, stDot, stDec, stCount, stError} eNumParseStates;


eNumParseStates stateTransition[stCount][256];

int NumberFromStringInitalised = 0;
void InitialiseNumberString()
{
	if(!NumberFromStringInitalised)
	{
		int i,j;

		for(j=0; j<stCount; j++)
		{
			for(i=0;i<256;i++)
			{
				stateTransition[j][i]	= stError;
			}
		}

		stateTransition[stSign]['-']	= stSign;
		stateTransition[stSign]['.']	= stDot;
		stateTransition[stInt]['.']		= stDot;

		for(i='0'; i<='9'; i++)
		{
			stateTransition[stSign][i]	= stInt;
			stateTransition[stInt][i]	= stInt;
			stateTransition[stDot][i]	= stDec;
			stateTransition[stDec][i]	= stDec;
		}

		NumberFromStringInitalised = 1;
	}
}


//------------------------------------------
// e.g. "73.2" "-45.34" ".2" "-.456"
//------------------------------------------
int NumberFromString(char* num, int decPlaces, long int* result)
{
	InitialiseNumberString();

	if(num)
	{
		int numlen = O_strlen(num);
		if(numlen)
		{
			long int sign = 1, mul = 1;
			char intpart[10];
			char decpart[10];
			int i, intindex=0, decindex=0;

			eNumParseStates parseState = stSign;

			for(i=0; i<numlen; i++)
			{
				parseState = stateTransition[parseState][(int)num[i]];

				switch(parseState)
				{
					case stSign:
						sign = -1;
						break;

					case stInt:
						intpart[intindex++] = num[i];
						break;

					case stDot:
						intpart[intindex] = 0;
						break;

					case stDec:
						decpart[decindex++] = num[i];
						break;

					case stError:
						return 0;

					default:
						break;
				}
			}

			while(decindex < decPlaces) decpart[decindex++] = '0';
			decpart[decPlaces] = 0;

			for(i=0;i<decPlaces;i++) mul *= 10;
			*result = sign*(atoi(intpart)*mul + atoi(decpart));

			return 1;
		}
	}

	return 0;
}


char* StringFromNumber(long int num, int decPlaces, int ignoreZeroDecPart)
{
	static char numbuffer[15];
	char intbuffer[10], decbuffer[10], digit;
	int decPartDigitCount, intPartDigitCount, n, sign=1;

	if(num < 0)
	{
		sign = -1;
		num = -num;
	}

	for(decPartDigitCount=0; decPartDigitCount<decPlaces; decPartDigitCount++)
	{
		digit = '0' + num%10;
		num /= 10;

		decbuffer[decPartDigitCount] = digit;
	}

	intPartDigitCount=0;
	while(num)
	{
		digit = '0' + num%10;
		num /= 10;

		intbuffer[intPartDigitCount++] = digit;
	}

	n = 0;
	if(sign < 0) numbuffer[n++] = '-';
	if(intPartDigitCount>0)
	{
		while(intPartDigitCount>0)
		{
			numbuffer[n++] = intbuffer[--intPartDigitCount];
		}
	}
	else
	{
		numbuffer[n++] = '0';
	}

	if( decPlaces>0 && (!ignoreZeroDecPart || atoi(decbuffer)>0) )
	{
		numbuffer[n++] = '.';
		while(decPartDigitCount>0)
		{
			numbuffer[n++] = decbuffer[--decPartDigitCount];
		}
	}
	numbuffer[n++] = 0;

	return numbuffer;
}



//#################################################################################################



int main()
{
    /* Some variables */
    char *temp = NULL;
    char *patchtext = NULL;
    char *patchedtext = NULL;

    int i = 0;
    /* JSON Pointer tests: */
    cJSON *root = NULL;
    const char *json=
        "{"
        "\"foo\": [\"bar\", \"baz\"],"
        "\"\": 0,"
        "\"a/b\": 1,"
        "\"c%d\": 2,"
        "\"e^f\": 3,"
        "\"g|h\": 4,"
        "\"i\\\\j\": 5,"
        "\"k\\\"l\": 6,"
        "\" \": 7,"
        "\"m~n\": 8"
        "}";

    const char* json2 =
    "{\"widget\": { \
    \"debug\": \"on\", \
    \"window\": [ \
		{ \
			\"title\": \"Sample Widget\", \
			\"name\": \"main_window\", \
			\"width\": 500, \
			\"height\": 500 \
		}, \
		{ \
			\"title\": \"Sample Widget\", \
			\"name\": \"sub_window\", \
			\"width\": 400, \
			\"height\": 400 \
		} \
    ], \
    \"image\": { \
        \"src\": \"Images/Sun.png\", \
        \"name\": \"sun1\", \
        \"hOffset\": 250, \
        \"vOffset\": 250, \
        \"alignment\": \"center\" \
    }, \
    \"text\": { \
        \"data\": \"Click Here\", \
        \"size\": 36, \
        \"style\": \"bold\", \
        \"name\": \"text1\", \
        \"hOffset\": 250, \
        \"vOffset\": 100, \
        \"alignment\": \"center\", \
        \"onMouseUp\": \"sun1.opacity = (sun1.opacity / 100) * 90;\" \
    } \
	}}";

	const char* json3 =
	"[\n"\
	"	{\n"\
	"	  \"fromDate\": \"2017-09-04\",\n"\
	"	  \"toDate\": \"2017-10-03\",\n"\
	"	  \"paymentDueDate\": \"2017-09-04\",\n"\
	"	  \"billPeriod\": \"M04\",\n"\
	"	  \"currency\": \"GBP\",\n"\
	"	  \"billingPeriodTotal\": 41.55,\n"\
	"	  \"details\": \"\",\n"\
	"	  \"extraCharges\":\n"\
	"	  [\n"\
	"	    {\n"\
	"	        \"transDate\": \"2017-09-04\",\n"\
	"	        \"amount\": 0.30,\n"\
	"	        \"description\": \"Credit Card Admin Charge\",\n"\
	"	        \"chargeId\": 13476\n"\
	"	    }\n"\
	"	  ]\n"\
	"	},\n"\
	"	{\n"\
	"	  \"fromDate\": \"2017-09-04\",\n"\
	"	  \"toDate\": \"2017-10-03\",\n"\
	"	  \"paymentDueDate\": \"2017-09-04\",\n"\
	"	  \"billPeriod\": \"M04\",\n"\
	"	  \"currency\": \"GBP\",\n"\
	"	  \"billingPeriodTotal\": 41.55,\n"\
	"	  \"details\": \"\",\n"\
	"	  \"extraCharges\":\n"\
	"	  [\n"\
	"	    {\n"\
	"	        \"transDate\": \"2017-09-04\",\n"\
	"	        \"amount\": 0.30,\n"\
	"	        \"description\": \"Credit Card Admin Charge\",\n"\
	"	        \"chargeId\": 13476\n"\
	"	    }\n"\
	"	  ]\n"\
	"	}\n"\
	"]\n";


	const char* json4 =
"["\
" {"\
"	\"currency\": \"GBP\","\
"	\"fromDate\": \"2017-09-04\","\
"	\"toDate\": \"2017-10-03\","\
"	\"paymentDueDate\": \"2017-09-04\","\
"	\"amount\": 41.55,"\
"	\"type\": \"FUTURE\""\
"  },"\
" {"\
"	\"currency\": \"GBP\","\
"	\"fromDate\": \"2017-10-04\","\
"	\"toDate\": \"2017-11-03\","\
"	\"paymentDueDate\": \"2017-10-04\","\
"	\"amount\": 41.55,"\
"	\"type\": \"FUTURE\""\
"  },"\
" {"\
"	\"currency\": \"GBP\","\
"	\"fromDate\": \"2017-11-04\","\
"	\"toDate\": \"2017-12-03\","\
"	\"paymentDueDate\": \"2017-11-04\","\
"	\"amount\": 41.55,"\
"	\"type\": \"FUTURE\""\
"  }"\
"]";


	const char* json5 =
"{"\
"	\"fromDate\": \"2017-09-04\","\
"	\"toDate\": \"2017-10-03\","\
"	\"paymentDueDate\": \"2017-09-04\","\
"	\"billPeriod\": \"M04\","\
"	\"currency\": \"GBP\","\
"	\"billingPeriodTotal\": 41.55,"\
"	\"details\":"\
"	{"\
"		\"subscriptionChargesTotal\": 41.25,"\
"		\"extraChargesTotal\": 0.3,"\
"		\"discCreditsTotal\": 0.0,"\
"		\"subscriptionCharges\":"\
"		["\
"			{"\
"				\"fromDate\": \"2017-09-04\","\
"				\"toDate\": \"2017-10-03\","\
"				\"category\": \"BB\","\
"				\"amount\": 5.0,"\
"				\"versionedProductId\": 142580001,"\
"				\"description\": \"Sky Broadband 12GB\""\
"			},"\
"			{"\
"				\"fromDate\": \"2017-09-04\","\
"				\"toDate\": \"2017-10-03\","\
"				\"category\": \"TALK\","\
"				\"amount\": 1.25,"\
"				\"versionedProductId\": 136230003,"\
"				\"description\": \"1571 - Voicemail\""\
"			},"\
"			{"\
"				\"fromDate\": \"2017-09-04\","\
"				\"toDate\": \"2017-10-03\","\
"				\"category\": \"TALK\","\
"				\"amount\": 0.0,"\
"				\"versionedProductId\": 140580001,"\
"				\"description\": \"Sky Pay As You Talk\""\
"			},"\
"			{"\
"				\"fromDate\": \"2017-09-04\","\
"				\"toDate\": \"2017-10-03\","\
"				\"category\": \"DTV\","\
"				\"amount\": 20.0,"\
"				\"versionedProductId\": 142450003,"\
"				\"description\": \"Original\""\
"			},"\
"			{"\
"				\"fromDate\": \"2017-09-04\","\
"				\"toDate\": \"2017-10-03\","\
"				\"category\": \"TALK\","\
"				\"amount\": 15.0,"\
"				\"undiscountedAmount\": 18.99,"\
"				\"versionedProductId\": 127210010,"\
"				\"description\": \"Sky Talk Line Rental\","\
"				\"offers\":"\
"				["\
"					{"\
"						\"fromDate\": \"2017-09-04\","\
"						\"toDate\": \"2017-10-03\","\
"						\"amount\": -3.99,"\
"						\"description\": \"Line Rental Discounted\","\
"						\"versionedOfferId\": 849460002"\
"					}"\
"				]"\
"			}"\
"		],"\
"		\"extraCharges\":"\
"		["\
"			{"\
"				\"transDate\": \"2017-09-04\","\
"				\"amount\": 0.3,"\
"				\"description\": \"Credit Card Admin Charge\","\
"				\"chargeId\": 13476"\
"			}"\
"		]"\
"	}"\
"}";



    const char *tests[12] = {"","/foo","/foo/0","/","/a~1b","/c%d","/e^f","/g|h","/i\\j","/k\"l","/ ","/m~0n"};
    const char* tests2[] = {"/widget/debug", "/widget/window", "/widget/window[0]", "/widget/window[0]/width", "/widget/window[1]/width"};
    const char* tests3[] = {"[0]/fromDate", "[0]/from"};

    /* JSON Apply Patch tests: */
    const char *patches[15][3] =
    {
        {"{ \"foo\": \"bar\"}", "[{ \"op\": \"add\", \"path\": \"/baz\", \"value\": \"qux\" }]","{\"baz\": \"qux\",\"foo\": \"bar\"}"},
        {"{ \"foo\": [ \"bar\", \"baz\" ] }", "[{ \"op\": \"add\", \"path\": \"/foo/1\", \"value\": \"qux\" }]","{\"foo\": [ \"bar\", \"qux\", \"baz\" ] }"},
        {"{\"baz\": \"qux\",\"foo\": \"bar\"}"," [{ \"op\": \"remove\", \"path\": \"/baz\" }]","{\"foo\": \"bar\" }"},
        {"{ \"foo\": [ \"bar\", \"qux\", \"baz\" ] }","[{ \"op\": \"remove\", \"path\": \"/foo/1\" }]","{\"foo\": [ \"bar\", \"baz\" ] }"},
        {"{ \"baz\": \"qux\",\"foo\": \"bar\"}","[{ \"op\": \"replace\", \"path\": \"/baz\", \"value\": \"boo\" }]","{\"baz\": \"boo\",\"foo\": \"bar\"}"},
        {"{\"foo\": {\"bar\": \"baz\",\"waldo\": \"fred\"},\"qux\": {\"corge\": \"grault\"}}","[{ \"op\": \"move\", \"from\": \"/foo/waldo\", \"path\": \"/qux/thud\" }]","{\"foo\": {\"bar\": \"baz\"},\"qux\": {\"corge\": \"grault\",\"thud\": \"fred\"}}"},
        {"{ \"foo\": [ \"all\", \"grass\", \"cows\", \"eat\" ] }","[ { \"op\": \"move\", \"from\": \"/foo/1\", \"path\": \"/foo/3\" }]","{ \"foo\": [ \"all\", \"cows\", \"eat\", \"grass\" ] }"},
        {"{\"baz\": \"qux\",\"foo\": [ \"a\", 2, \"c\" ]}","[{ \"op\": \"test\", \"path\": \"/baz\", \"value\": \"qux\" },{ \"op\": \"test\", \"path\": \"/foo/1\", \"value\": 2 }]",""},
        {"{ \"baz\": \"qux\" }","[ { \"op\": \"test\", \"path\": \"/baz\", \"value\": \"bar\" }]",""},
        {"{ \"foo\": \"bar\" }","[{ \"op\": \"add\", \"path\": \"/child\", \"value\": { \"grandchild\": { } } }]","{\"foo\": \"bar\",\"child\": {\"grandchild\": {}}}"},
        {"{ \"foo\": \"bar\" }","[{ \"op\": \"add\", \"path\": \"/baz\", \"value\": \"qux\", \"xyz\": 123 }]","{\"foo\": \"bar\",\"baz\": \"qux\"}"},
        {"{ \"foo\": \"bar\" }","[{ \"op\": \"add\", \"path\": \"/baz/bat\", \"value\": \"qux\" }]",""},
        {"{\"/\": 9,\"~1\": 10}","[{\"op\": \"test\", \"path\": \"/~01\", \"value\": 10}]",""},
        {"{\"/\": 9,\"~1\": 10}","[{\"op\": \"test\", \"path\": \"/~01\", \"value\": \"10\"}]",""},
        {"{ \"foo\": [\"bar\"] }","[ { \"op\": \"add\", \"path\": \"/foo/-\", \"value\": [\"abc\", \"def\"] }]","{\"foo\": [\"bar\", [\"abc\", \"def\"]] }"}
    };

    /* JSON Apply Merge tests: */
    const char *merges[15][3] =
    {
        {"{\"a\":\"b\"}", "{\"a\":\"c\"}", "{\"a\":\"c\"}"},
        {"{\"a\":\"b\"}", "{\"b\":\"c\"}", "{\"a\":\"b\",\"b\":\"c\"}"},
        {"{\"a\":\"b\"}", "{\"a\":null}", "{}"},
        {"{\"a\":\"b\",\"b\":\"c\"}", "{\"a\":null}", "{\"b\":\"c\"}"},
        {"{\"a\":[\"b\"]}", "{\"a\":\"c\"}", "{\"a\":\"c\"}"},
        {"{\"a\":\"c\"}", "{\"a\":[\"b\"]}", "{\"a\":[\"b\"]}"},
        {"{\"a\":{\"b\":\"c\"}}", "{\"a\":{\"b\":\"d\",\"c\":null}}", "{\"a\":{\"b\":\"d\"}}"},
        {"{\"a\":[{\"b\":\"c\"}]}", "{\"a\":[1]}", "{\"a\":[1]}"},
        {"[\"a\",\"b\"]", "[\"c\",\"d\"]", "[\"c\",\"d\"]"},
        {"{\"a\":\"b\"}", "[\"c\"]", "[\"c\"]"},
        {"{\"a\":\"foo\"}", "null", "null"},
        {"{\"a\":\"foo\"}", "\"bar\"", "\"bar\""},
        {"{\"e\":null}", "{\"a\":1}", "{\"e\":null,\"a\":1}"},
        {"[1,2]", "{\"a\":\"b\",\"c\":null}", "{\"a\":\"b\"}"},
        {"{}","{\"a\":{\"bb\":{\"ccc\":null}}}", "{\"a\":{\"bb\":{}}}"}
    };


    /* Misc tests */
    int numbers[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    const char *random = "QWERTYUIOPASDFGHJKLZXCVBNM";
    char buf[2] = {0,0};
    char *before = NULL;
    char *after = NULL;
    cJSON *object = NULL;
    cJSON *nums = NULL;
    cJSON *num6 = NULL;
    cJSON *sortme = NULL;


    //######################################################################


    root = cJSON_Parse(json2);
    printf("%s\n\n", cJSON_Print(root));
    for (i = 0; i < 5; i++)
    {
        char *output = cJSON_Print(cJSON_GetPointer(root, tests2[i]));
        printf("Test %d:\n%s = %s\n\n", i+1, tests2[i], output);
        free(output);
    }
    cJSON_Delete(root);



    root = cJSON_Parse(json3);
    printf("%s\n\n", cJSON_Print(root));
    for (i = 0; i < 2; i++)
    {
        char *output = cJSON_Print(cJSON_GetPointer(root, tests3[i]));
        printf("Test %d:\n%s = %s\n\n", i+1, tests3[i], output);
        free(output);
    }
    cJSON_Delete(root);


    root = cJSON_Parse(json4);
    printf("%s\n\n", cJSON_Print(root));
    cJSON_Delete(root);

    root = cJSON_Parse(json5);
    printf("%s\n\n", cJSON_Print(root));
    cJSON_Delete(root);


    {
    	#define DECIMALS 2
    	long int result = 0;
    	char *tests[] = { "73.20", "-73.2", "-.2", "0.3", "-0.455", ".45", "56", "fg", "45g", "2..2", "2.2.2", "2."};

    	for(i=0; i<sizeof(tests)/sizeof(char*); i++)
		{
			if(NumberFromString(tests[i], DECIMALS, &result))
			{
				printf("\n\nParsed successfully: %s -> %ld -> %s\n\n", tests[i], result, StringFromNumber(result, DECIMALS, 0));
			}
			else
			{
				printf("\nNot a valid number: %s\n\n", tests[i]);
			}
		}
	}


    //######################################################################


    FILE* fp = fopen("test.json", "r+");
    {
    	const int max_size = 50000;
    	char buffer[max_size+1];

    	int len = fread(buffer, 1, max_size, fp);
    	buffer[len]=0;

    	root = cJSON_Parse(buffer);

    	printf("\n############################################\nTest JSON file %s parsed!\n############################################\n", root ? "successfully" : "unsuccessfully");
    }
    fclose(fp);

    return 0;

    printf("JSON Pointer Tests\n");
    root = cJSON_Parse(json);
    for (i = 0; i < 12; i++)
    {
        char *output = cJSON_Print(cJSONUtils_GetPointer(root, tests[i]));
        printf("Test %d:\n%s\n\n", i + 1, output);
        free(output);
    }
    cJSON_Delete(root);


    printf("JSON Apply Patch Tests\n");
    for (i = 0; i < 15; i++)
    {
        cJSON *object_to_be_patched = cJSON_Parse(patches[i][0]);
        cJSON *patch = cJSON_Parse(patches[i][1]);
        int err = cJSONUtils_ApplyPatches(object_to_be_patched, patch);
        char *output = cJSON_Print(object_to_be_patched);
        printf("Test %d (err %d):\n%s\n\n", i + 1, err, output);

        free(output);
        cJSON_Delete(object_to_be_patched);
        cJSON_Delete(patch);
    }

    /* JSON Generate Patch tests: */
    printf("JSON Generate Patch Tests\n");
    for (i = 0; i < 15; i++)
    {
        cJSON *from;
        cJSON *to;
        cJSON *patch;
        char *out;
        if (!strlen(patches[i][2]))
        {
            continue;
        }
        from = cJSON_Parse(patches[i][0]);
        to = cJSON_Parse(patches[i][2]);
        patch = cJSONUtils_GeneratePatches(from, to);
        out = cJSON_Print(patch);
        printf("Test %d: (patch: %s):\n%s\n\n", i + 1, patches[i][1], out);

        free(out);
        cJSON_Delete(from);
        cJSON_Delete(to);
        cJSON_Delete(patch);
    }

    /* Misc tests: */
    printf("JSON Pointer construct\n");
    object = cJSON_CreateObject();
    nums = cJSON_CreateIntArray(numbers, 10);
    num6 = cJSON_GetArrayItem(nums, 6);
    cJSON_AddItemToObject(object, "numbers", nums);
    temp = cJSONUtils_FindPointerFromObjectTo(object, num6);
    printf("Pointer: [%s]\n", temp);
    free(temp);
    temp = cJSONUtils_FindPointerFromObjectTo(object, nums);
    printf("Pointer: [%s]\n", temp);
    free(temp);
    temp = cJSONUtils_FindPointerFromObjectTo(object, object);
    printf("Pointer: [%s]\n", temp);
    free(temp);
    cJSON_Delete(object);

    /* JSON Sort test: */
    sortme = cJSON_CreateObject();
    for (i = 0; i < 26; i++)
    {
        buf[0] = random[i];
        cJSON_AddItemToObject(sortme, buf, cJSON_CreateNumber(1));
    }
    before = cJSON_PrintUnformatted(sortme);
    cJSONUtils_SortObject(sortme);
    after = cJSON_PrintUnformatted(sortme);
    printf("Before: [%s]\nAfter: [%s]\n\n", before, after);

    free(before);
    free(after);
    cJSON_Delete(sortme);

    /* Merge tests: */
    printf("JSON Merge Patch tests\n");
    for (i = 0; i < 15; i++)
    {
        cJSON *object_to_be_merged = cJSON_Parse(merges[i][0]);
        cJSON *patch = cJSON_Parse(merges[i][1]);
        char *before_merge = cJSON_PrintUnformatted(object_to_be_merged);
        patchtext = cJSON_PrintUnformatted(patch);
        printf("Before: [%s] -> [%s] = ", before_merge, patchtext);
        object_to_be_merged = cJSONUtils_MergePatch(object_to_be_merged, patch);
        after = cJSON_PrintUnformatted(object_to_be_merged);
        if(after) printf("[%s] vs [%s] (%s)\n", after, merges[i][2], strcmp(after, merges[i][2]) ? "FAIL" : "OK");

        free(before_merge);
        free(patchtext);
        free(after);
        cJSON_Delete(object_to_be_merged);
        cJSON_Delete(patch);
    }

    /* Generate Merge tests: */
    for (i = 0; i < 15; i++)
    {
        cJSON *from = cJSON_Parse(merges[i][0]);
        cJSON *to = cJSON_Parse(merges[i][2]);
        cJSON *patch = cJSONUtils_GenerateMergePatch(from,to);
        from = cJSONUtils_MergePatch(from,patch);
        patchtext = cJSON_PrintUnformatted(patch);
        patchedtext = cJSON_PrintUnformatted(from);
        if(patchedtext) printf("Patch [%s] vs [%s] = [%s] vs [%s] (%s)\n", patchtext, merges[i][1], patchedtext, merges[i][2], strcmp(patchedtext, merges[i][2]) ? "FAIL" : "OK");

        cJSON_Delete(from);
        cJSON_Delete(to);
        cJSON_Delete(patch);
        free(patchtext);
        free(patchedtext);
    }

    return 0;
}
