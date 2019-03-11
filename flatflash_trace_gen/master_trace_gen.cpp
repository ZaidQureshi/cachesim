
#include <iostream>
#include <random>
#include <cmath>

#include  <iomanip>

#define KILO 1024ULL
#define MEGA (1024ULL * KILO)
#define GIGA (1024ULL * MEGA)



#define MIN 0
#define MAX (1 * GIGA)

#define LOCAL (512 * MEGA)

#define PERCENT_WRITE 0.05

#define N 100000000ULL
#define N1 40000000ULL
#define N2 40000000ULL
#define N3 20000000ULL

#define BS 64

#define ALPHA 100

#define LAMBDA 1.0


#define  FALSE          0       // Boolean false
#define  TRUE           1       // Boolean true

using namespace std;

long double rand_val(int seed)
{
  const long  a =      16807;  // Multiplier
  const long  m = 2147483647;  // Modulus
  const long  q =     127773;  // m div a
  const long  r =       2836;  // m mod a
  static long x;               // Random int value
  long        x_div_q;         // x divided by q
  long        x_mod_q;         // x modulo q
  long        x_new;           // New x value

  // Set the seed if argument is non-zero and then return zero
  if (seed > 0)
  {
    x = seed;
    return(0.0);
  }

  // RNG using integer arithmetic
  x_div_q = x / q;
  x_mod_q = x % q;
  x_new = (a * x_mod_q) - (r * x_div_q);
  if (x_new > 0)
    x = x_new;
  else
    x = x_new + m;

  // Return a random value between 0.0 and 1.0
  return((long double) x / m);
}

unsigned long long zipf(double alpha, unsigned long long n)
{
  static int first = TRUE;      // Static first time flag
  static long double c = 0;          // Normalization constant
  long double z;                     // Uniform random number (0 < z < 1)
  long double sum_prob;              // Sum of probabilities
  long double zipf_value;            // Computed exponential value to be returned
  unsigned long long    i;                     // Loop counter

  // Compute normalization constant on first call only
  if (first == TRUE)
  {
    for (i=1; i<=n; i++)
      c = c + (1.0 / pow((long double) i, alpha));
    c = 1.0 / c;
    first = FALSE;
  }

  // Pull a uniform random number (0 < z < 1)
  do
  {
    z = rand_val(0);
  }
  while ((z == 0) || (z == 1));

  // Map z to the value
  sum_prob = 0;
  for (i=1; i<=n; i++)
  {
    sum_prob = sum_prob + c / pow((long double) i, alpha);
    if (sum_prob >= z)
    {
      zipf_value = i;
      break;
    }
  }

  // Assert that zipf_value is between 1 and N
  //assert((zipf_value >=1) && (zipf_value <= n));

  return(zipf_value);
}


int main(int argc, char *argv[]) {
  unsigned long long gigs = MAX;
  double write_percent = PERCENT_WRITE;
  string dist;
  if (argc > 1)
    gigs *= stoul(argv[1], nullptr, 10);
  if (argc > 2)
    write_percent = stod(argv[2]);
  if (argc > 3)
    dist = string(argv[3]);
  
  default_random_engine rdwr_generator;
  uniform_real_distribution<double> rdwr_distribution(0.0,1.0);

  //unsigned int shift = (unsigned int) ceil(log2(BS));
  
  if (dist == "uniform") {
    default_random_engine addr_generator;
    uniform_int_distribution<unsigned long long> addr_distribution(MIN,gigs);

    default_random_engine addr_generator2;
    uniform_int_distribution<unsigned long long> addr_distribution2(MIN,LOCAL);

    
  
    for (unsigned long long count = 0; count < N1; count++) {
      double rdwr = rdwr_distribution(rdwr_generator);
      if (rdwr <= write_percent)
	cout << "S\t";
      else
	cout << "L\t";

      unsigned long long addr = addr_distribution(addr_generator);
      //cout << setfill('0') << setw(16) << hex << (addr << shift) << endl;
      cout  << hex << (addr) << endl;
    
    }

    for (unsigned long long count = 0; count < N2; count++) {
      double rdwr = rdwr_distribution(rdwr_generator);
      if (rdwr <= write_percent)
	cout << "S\t";
      else
	cout << "L\t";

      unsigned long long addr = addr_distribution2(addr_generator2);
      //cout << setfill('0') << setw(16) << hex << (addr << shift) << endl;
      cout  << hex << (addr) << endl;
    
    }

    for (unsigned long long count = 0; count < N3; count++) {
      double rdwr = rdwr_distribution(rdwr_generator);
      if (rdwr <= write_percent)
	cout << "S\t";
      else
	cout << "L\t";

      unsigned long long addr = addr_distribution(addr_generator);
      //cout << setfill('0') << setw(16) << hex << (addr << shift) << endl;
      cout  << hex << (addr) << endl;
    
    }
    
  }

  else if (dist == "binomial") {
    default_random_engine addr_generator;
    //poisson_distribution<unsigned long long> addr_distribution(LAMBDA);
    binomial_distribution<unsigned long long> addr_distribution(gigs, 0.5);
    for (unsigned long long count = 0; count < N; count++) {
      double rdwr = rdwr_distribution(rdwr_generator);
      if (rdwr <= write_percent)
	cout << "S\t";
      else
	cout << "L\t";

      unsigned long long addr = addr_distribution(addr_generator);
      //cout << setfill('0') << setw(16) << hex << (addr << shift) << endl;
      cout  << hex << (addr) << endl;
    
    }

  }
  

  

}
