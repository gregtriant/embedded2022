/* 

1. scan for Bluetooth MAC addresses (every 10 sec --> return a 48bit value)
2. Define close contacts for each MAC address: a MAC address found in scans between 4 and 20 mins
3. Save close contacts for 14 days
4. Remeber each MAC address for 20 mins. IF there are some that are not close contacts, forget them after 20 mins.



*/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct macaddress {
    char address[];
} macaddress;

// ----------------------------------  Function declerations  ----------------------------------
macaddress BTnearMe();
bool testCOVID();
void uploadContacts(macaddress*, int);


int main() {
    BTnearMe();
    return 0;
}


// --------------------------------------  Functions  -------------------------------------------

macaddress BTnearMe() {
    
}


