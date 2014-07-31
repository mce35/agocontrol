#include <string.h>
#include <limits.h> //PATH_MAX
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>

#include "mongoose.h"

#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

using namespace std;


//code from https://github.com/cesanta/mongoose/blob/master/examples/server.c#L339
int modify_passwords_file(const char *fname, const char *domain,
                          const char *user, const char *pass) {
  int found;
  char line[512], u[512], d[512], ha1[33], tmp[PATH_MAX];
  FILE *fp, *fp2;

  found = 0;
  fp = fp2 = NULL;

  // Regard empty password as no password - remove user record.
  if (pass != NULL && pass[0] == '\0') {
    pass = NULL;
  }

  (void) snprintf(tmp, sizeof(tmp), "%s.tmp", fname);

  // Create the file if does not exist
  if ((fp = fopen(fname, "a+")) != NULL) {
    fclose(fp);
  }

  // Open the given file and temporary file
  if ((fp = fopen(fname, "r")) == NULL) {
    return 0;
  } else if ((fp2 = fopen(tmp, "w+")) == NULL) {
    fclose(fp);
    return 0;
  }

  // Copy the stuff to temporary file
  while (fgets(line, sizeof(line), fp) != NULL) {
    if (sscanf(line, "%[^:]:%[^:]:%*s", u, d) != 2) {
      continue;
    }

    if (!strcmp(u, user) && !strcmp(d, domain)) {
      found++;
      if (pass != NULL) {
        mg_md5(ha1, user, ":", domain, ":", pass, NULL);
        fprintf(fp2, "%s:%s:%s\n", user, domain, ha1);
      }
    } else {
      fprintf(fp2, "%s", line);
    }
  }

  // If new user, just add it
  if (!found && pass != NULL) {
    mg_md5(ha1, user, ":", domain, ":", pass, NULL);
    fprintf(fp2, "%s:%s:%s\n", user, domain, ha1);
  }

  // Close files
  fclose(fp);
  fclose(fp2);

  // Put the temp file in place of real file
  remove(fname);
  rename(tmp, fname);

  return 1;
}

int main(int argc, char **argv) {
	if (argc != 5) {
		cout << "Usage: " <<  argv[0] << " <filename> <domainname> <username> <password>" << endl;
		exit(-1);
	}

	modify_passwords_file(argv[1], argv[2], argv[3], argv[4]);
}
