#include "revert_string.h"
#include <string.h>

void RevertString(char *str)
{
	int len = strlen(str);
	char t[len];
	int j = 0; 
	for (int i = len - 1;  i>=0; i--){
		t[j] = str[i];
		j++;
	}
	for (int i = 0; i < len; i++)
	{
		str[i] = t[1];
	}

}

