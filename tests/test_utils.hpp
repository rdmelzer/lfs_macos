#pragma once

void Mklfs(char flashFile[])
{
	char command[80];
	strcpy(command, "./mklfs ");
	strcat(command, flashFile);
	system(command);
}

void DeleteTestFlash(char flashFile[])
{
	char command[80];
	strcpy(command, "rm ");
	strcat(command, flashFile);
	system(command);
}