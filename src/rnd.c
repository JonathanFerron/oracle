#include <stdint.h>

#include "rnd.h"
#include "mtwister.h"

// Global variables (keep to a minimum)
MTRand MTwister_rand_struct;

//uint8_t d4() { return dn(4);}
//uint8_t d6() { return dn(6);}
//uint8_t d8() { return dn(8);}
//uint8_t d12() { return dn(12);}
//uint8_t d20() { return dn(20);}

// return a pseudo-random integer number between 1 and n
uint8_t RND_dn(uint8_t n) {
  return RND_randn(n) + 1;
} // dn

// randn will return a pseudo-random integer number between 0 and n-1
uint8_t RND_randn(uint8_t n) {
  return (uint8_t)genRandLong(&MTwister_rand_struct) % n;
} // randn

// A function to generate a random permutation of arr[]
/* to use this:
     int arr[] = {1, 2, 3, 4, 5, 6, 7, 8};
     int n = sizeof(arr)/ sizeof(arr[0]);
     shuffle_array(arr, n);
*/
void RND_shuffle_array( uint8_t arr[], uint8_t n )
{
  // Start from the last element and swap one by one. We don't
  // need to run for the first element that's why i > 0
  for (uint8_t i = n-1; i > 0; i--)
  {
    // Pick a random index from 0 to i
    uint8_t j = RND_randn(i+1);

    // Swap arr[i] with the element at random index
    RND_swap(&arr[i], &arr[j]);
  }
} // shuffle_array()

// A utility function to swap two integers via pointers
void RND_swap (uint8_t *a, uint8_t *b)
{
  uint8_t temp = *a;
  *a = *b;
  *b = temp;
}

// need to double check the logic to ensure proper implementation, n=size of array, k <= n
void RND_partial_shuffle(uint8_t A[], uint8_t n, uint8_t k)
{
  uint8_t j;
  for (uint8_t i = 0; i < k; i++)
  {
    j = i + RND_randn(n - i);
    RND_swap(&A[i], &A[j]);
  }
} // partial shuffle
