#ifndef STRING_SORT_H_
#define STRING_SORT_H_

#include "daisy_patch_sm.h"

// swap arr[x] <-> array[y]
void stringSwap(char **arr, int x, int y) {
    char *temp;
    temp = arr[x];
    arr[x] = arr[y];
    arr[y] = temp;
}

int partition(char **arr, int low, int high) {
    // Choose the pivot
    int pivot = high;
  
    // Index of smaller element and indicates 
    // the right position of pivot found so far
    int i = low - 1;

    // Traverse arr[low..high] and move all smaller
    // elements on left side. Elements from low to 
    // i are smaller after every iteration
    for (int j = low; j <= high - 1; j++) {
        if (strcmp(arr[j], arr[pivot]) < 0) {
            i++;
            stringSwap(arr, i, j);
        }
    }
    
    // Move pivot after smaller elements and
    // return its position
    stringSwap(arr, i + 1, high);  
    return i + 1;
}

// The QuickSort function implementation
void quickSort(char **arr, int low, int high) {
    if (low < high) {
      
        // pi is the partition return index of pivot
        int pi = partition(arr, low, high);

        // Recursion calls for smaller elements
        // and greater or equals elements
        quickSort(arr, low, pi - 1);
        quickSort(arr, pi + 1, high);
    }
}

void stringSort(char **array, int size) {
    quickSort(array, 0, size-1);
}

#endif