#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "cross_platform.h"

#define SIZE_LINE		1024	//ÿ����󳤶�
#define SIZE_FILENAME	256		//�ļ�����󳤶�

#define min(x, y)		(x <= y) ? x : y

typedef enum _ELineType_ {
    LINE_IDLE,		//δ������
	LINE_ERROR,		//������
	LINE_EMPTY,		//�հ��л�ע����
	LINE_SECTION,	//�ڶ�����
	LINE_VALUE		//ֵ������
} ELineType ;


static char gFilename[SIZE_FILENAME];
static char *gBuffer;
static int32_t gBuflen;


//ȥ������β�ո�ԭ������д
static char *StrStrip(char *s)
{
	size_t size;
	char *p1, *p2;

	size = strlen(s);
	if (!size)
		return s;

	p2 = s + size - 1;

	while ((p2 >= s) && isspace(*p2))
		p2 --;
	*(p2 + 1) = '\0';

	p1 = s;
	while (*p1 && isspace(*p1))
		p1 ++;
	if (s != p1)
		memmove(s, p1, p2 - p1 + 2);
	return s;
}


//�����ִ�Сд�Ƚ��ַ���
static int32_t StriCmp(const char *s1, const char *s2)
{
	int32_t ch1, ch2;
	do
	{
		ch1 = (unsigned char)*(s1++);
		if ((ch1 >= 'A') && (ch1 <= 'Z'))
			ch1 += 0x20;

		ch2 = (unsigned char)*(s2++);
		if ((ch2 >= 'A') && (ch2 <= 'Z'))
			ch2 += 0x20;
	} while ( ch1 && (ch1 == ch2) );
	return(ch1 - ch2);
}


//ȡһ��
//���룺������(ָ�뼰����)
//����������͡���Ч���ݴ�(ȥ��β�ո�)��ע���ס�ע��β����һ����(��β����һ���׼�Ϊ���з�)
//      ��Ч����λ��Ϊ[buf, rem1)
static int32_t GetLine(char *buf, int32_t buflen, char *content, char **rem1, char **rem2, char **nextline)
{
	char *cont1, *cont2;
	int32_t cntblank, cntCR, cntLF;		//�����ո񡢻��з�����
	int8_t isQuot1, isQuot2;			//����
	int32_t i;
	char *p;

	//���ȶ��ж�ע�ͣ�֧�����»��з���\r��\n��\r\n��\n\r
	cntblank = 0;
	cntCR = cntLF = 0;
	isQuot1 = isQuot2 = 0;
	cont1 = *rem1 = 0;
	content[0] = 0;
	for (i = 0, p = buf; i < buflen; i ++, p ++)
	{
		if (*p == 0) {
			p ++;
			break;
		}
		//2��CR��LF���н���
		if (cntCR == 2 || cntLF == 2) {
			p --;	//����1
			break;
		}
		//CR��LF��1��֮�������ַ����н���
		if (cntCR + cntLF >= 2) {
			break;
		}
		//CR��LF֮����������ַ����н���
		if ((cntCR || cntLF) && *p != '\r' && *p != '\n')
			break;

		switch (*p) {
		case '\r':
			cntCR ++;
			break;
		case '\n':
			cntLF ++;
			break;
		case '\'':
			if (!isQuot2)
				isQuot1 = 1 - isQuot1;
			break;
		case '\"':
			if (!isQuot1)
				isQuot2 = 1 - isQuot2;
			break;
		case ';':
		case '#':
			if (isQuot1 || isQuot2)
				break;
			if (*rem1 == NULL)
				*rem1 = p - cntblank;
			break;
		default:
			if (isspace((unsigned char)*p)) {
				cntblank ++;
			} else {
				cntblank = 0;
				if ((*rem1 == NULL) && (cont1 == NULL))
					cont1 = p;
			}
			break;
		}
	}

	*nextline = p;
	*rem2 = p - cntCR - cntLF;
	if (*rem1 == NULL)
		*rem1 = *rem2;
	cont2 = *rem1 - cntblank;

	if (cont1 == NULL) {
		cont1 = cont2;
		return LINE_EMPTY;
	}

	i = (int)(cont2 - cont1);
	if (i >= SIZE_LINE)
		return LINE_ERROR;

	//����ͷβ���޿ո�
	memcpy(content, cont1, i);
	content[i] = 0;

	if (content[0] == '[' && content[i - 1] == ']')
		return LINE_SECTION;
	if (strchr(content, '=') != NULL)
		return LINE_VALUE;
	
	return LINE_ERROR;
}


//ȡһ��section
//���룺������
//������ɹ���񡢽������ס�������β���������ס�������β(������)����һ����(��β����һ���׼�Ϊ���л�ע����)
static int32_t FindSection(const char *section, char **sect1, char **sect2, char **cont1, char **cont2, char **nextsect)
{
	int32_t type;
	char content[SIZE_LINE];
	char *rem1 = NULL, *rem2 = NULL, *nextline = NULL;

	char *p;
	char *empty;
	int32_t uselen = 0;
	int8_t found = 0;

	if (gBuffer == NULL) {
		return 0;
	}

	while (gBuflen - uselen > 0) {
		p = gBuffer + uselen;
		type = GetLine(p, gBuflen - uselen, content, &rem1, &rem2, &nextline);
		uselen += (int32_t)(nextline - p);

		if (LINE_SECTION == type) {
			if (found || section == NULL) break;		//������һsection
			content[strlen(content) - 1] = 0;			//ȥβ��]
			StrStrip(content + 1);						//ȥ��β�ո�
			if (StriCmp(content + 1, section) == 0) {
				found = 1;
				*sect1 = p;
				*sect2 = rem1;
				*cont1 = nextline;
			}
			empty = nextline;
		} else
		if (LINE_VALUE == type) {
			if (!found && section == NULL) {
				found = 1;
				*sect1 = p;
				*sect2 = p;
				*cont1 = p;
			}
			empty = nextline;
		}
	}
	
	if (!found) return 0;

	*cont2 = empty;
	*nextsect = nextline;
	return 1;
}


//��һ��ȡ����ֵ
//���룺���ݴ�(������д)
//�����������ֵ��
static void GetKeyValue(char *content, char **key, char **value)
{
	char *p;

	p = strchr(content, '=');
	*p = 0;
	StrStrip(content);
	StrStrip(p + 1);
	*key = content;
	*value = p + 1;
}


//�ͷ�ini�ļ���ռ��Դ
void iniFileFree()
{
	if (gBuffer != NULL) {
		free(gBuffer);
		gBuffer = 0;
		gBuflen = 0;
	}
}


//����ini�ļ����ڴ�
int32_t iniFileLoad(const char *filename)
{
	FILE *file;
	int len;

	iniFileFree();
	if (strlen(filename) >= sizeof(gFilename))
		return 0;
	strcpy(gFilename, filename);

	file = fopen(gFilename, "rb");
	if (file == NULL) 
		return 0;

	fseek(file, 0, SEEK_END);
	len = ftell(file);
	gBuffer = (char *)malloc(len);
	if (gBuffer == NULL) {
		fclose(file);
		return 0;
	}

	fseek(file, 0, SEEK_SET);
	len = fread(gBuffer, 1, len, file);
	fclose(file);
	gBuflen = len;
	return 1;
}


//��ȡֵԭʼ��
static int32_t iniGetValue(const char *section, const char *key, char *value, int32_t maxlen, const char *defvalue)
{
	int32_t type;
	char content[SIZE_LINE];
	char *rem1, *rem2, *nextline;
	char *key0, *value0;

	char *p;
	int32_t uselen = 0;
	int8_t found = 0;

	int32_t len;

	if (gBuffer == NULL || key == NULL) {
		if (value != NULL)
			value[0] = 0;
		return 0;
	}

	while (gBuflen - uselen > 0) {
		p = gBuffer + uselen;
		type = GetLine(p, gBuflen - uselen, content, &rem1, &rem2, &nextline);
		uselen += (int32_t)(nextline - p);

		if (LINE_SECTION == type) {
			if (found || section == NULL) break;		//������һsection
			content[strlen(content) - 1] = 0;			//ȥβ��]
			StrStrip(content + 1);						//ȥ��β�ո�
			if (StriCmp(content + 1, section) == 0) {
				found = 1;
			}
		} else
		if (LINE_VALUE == type) {
			if (!found && section == NULL) {
				found = 1;
			}
			if (!found)
				continue;
			GetKeyValue(content, &key0, &value0);
			if (StriCmp(key0, key) == 0) {
				len = strlen(value0);
				if (len == 0) break;		//��ֵ��Ϊ��Ч
				if (value != NULL) {
					len = min(len, maxlen - 1);
					strncpy(value, value0, len);
					value[len] = 0;
				}
				return 1;
			}
		}
	}
	
	//δ���ּ�ֵȡȱʡ
	if (value != NULL) {
		if (defvalue != NULL) {
			len = min(strlen(defvalue), maxlen - 1);
			strncpy(value, defvalue, len);
			value[len] = 0;
		} else {
			value[0] = 0;
		}
	}
	return 0;
}


//��ȡ�ַ�������������
int32_t iniGetString(const char *section, const char *key, char *value, int32_t maxlen, const char *defvalue)
{
	int32_t ret;
	int32_t len;

	ret = iniGetValue(section, key, value, maxlen, defvalue);
	if (!ret)
		return ret;

	//ȥ��β�ո�
	len = strlen(value);
	if (value[0] == '\'' && value[len - 1] == '\'') {
		value[len - 1] = 0;
		memmove(value, value + 1, len - 1);
	} else
	if (value[0] == '\"' && value[len - 1] == '\"') {
		value[len - 1] = 0;
		memmove(value, value + 1, len - 1);
	}
	return ret;
}


//��ȡ����ֵ
int32_t iniGetInt(const char *section, const char *key, int32_t defvalue)
{
	char valstr[64];

	if (iniGetValue(section, key, valstr, sizeof(valstr), NULL))
	    return (int32_t)strtol(valstr, NULL, 0);
	return defvalue;
}


//��ȡ������
double iniGetDouble(const char *section, const char *key, double defvalue)
{
	char valstr[64];

	if (iniGetValue(section, key, valstr, sizeof(valstr), NULL))
	    return (double)atof(valstr);
	return defvalue;
}


//�����ַ�������valueΪNULL����ɾ����key�����У�����ע��
int32_t iniSetString(const char *section, const char *key, const char *value)
{
	FILE *file;
	char *sect1, *sect2, *cont1, *cont2, *nextsect;
	char *p;
	int32_t len, type;
	char content[SIZE_LINE];
	char *key0, *value0;
	char *rem1, *rem2, *nextline;


	if (gBuffer == NULL) {
		return 0;
	}

	if (FindSection(section, &sect1, &sect2, &cont1, &cont2, &nextsect) == 0)
	{
		//δ�ҵ���

		//value��Ч�򷵻�
		if (value == NULL) 
			return 0;

		//���ļ�β������
		file = fopen(gFilename, "ab");
		if (file == NULL) 
			return 0;
		fprintf(file, "\n[%s]\n%s = %s\n", section, key, value);
		fclose(file);
		iniFileLoad(gFilename);
		return 1;
	}

	//�ҵ��ڣ�����ڲ���key
	p = cont1;
	len = (int32_t)(cont2 - cont1);
	while (len > 0) {
		type = GetLine(p, len, content, &rem1, &rem2, &nextline);

		if (LINE_VALUE == type) {
			GetKeyValue(content, &key0, &value0);
			if (StriCmp(key0, key) == 0) {
				//�ҵ�key
				file = fopen(gFilename, "wb");
				if (file == NULL) 
					return 0;
				len = (int32_t)(p - gBuffer);
				fwrite(gBuffer, 1, len, file);					//д��key֮ǰ����
				if (value == NULL) {
					//value��Ч��ɾ��
					len = (int32_t)(nextline - gBuffer);			//������ͬע��һ��ɾ��
				} else {
					//value��Ч����д
					fprintf(file, "%s = %s", key, value);
					len = (int32_t)(rem1 - gBuffer);				//����β��ԭע��!
				}
				fwrite(gBuffer + len, 1, gBuflen - len, file);	//д��key�����к�ע��֮�󲿷�
				fclose(file);
				iniFileLoad(gFilename);
				return 1;
			}	
		}

		len -= (int32_t)(nextline - p);
		p = nextline;
	}

	//δ�ҵ�key

	//value��Ч�򷵻�
	if (value == NULL) 
		return 0;

	//���ļ�β������
	file = fopen(gFilename, "wb");
	if (file == NULL) 
		return 0;
	len = (int32_t)(cont2 - gBuffer);
	fwrite(gBuffer, 1, len, file);					//д��key֮ǰ����
	fprintf(file, "%s = %s\n", key, value);
	fwrite(gBuffer + len, 1, gBuflen - len, file);	//д��key֮�󲿷�
	fclose(file);
	iniFileLoad(gFilename);
	return 1;
}


//��������ֵ��baseȡֵ10��16��8���ֱ��ʾ10��16��8���ƣ�ȱʡΪ10����
int32_t iniSetInt(const char *section, const char *key, int32_t value, int32_t base)
{
	char valstr[64];

	switch (base) {
	case 16:
		sprintf(valstr, "0x%x", value);
		return iniSetString(section, key, valstr);
	case 8:
		sprintf(valstr, "0%o", value);
		return iniSetString(section, key, valstr);
	default:	//10
		sprintf(valstr, "%d", value);
		return iniSetString(section, key, valstr);
	}
}