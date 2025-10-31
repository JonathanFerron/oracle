I'll provide the mathematical formulas and then create a visualization for the maximum of two dice rolls.

## Mathematical Analysis

### General Formulas for M = max(d₁(n), d₂(n))

**Probability Distribution Function:**
```
P(M = k) = P(max = k) = P(both ≤ k) - P(both ≤ k-1)
         = (k/n)² - ((k-1)/n)²
         = (2k - 1)/n²
```

for k = 1, 2, ..., n

**Cumulative Distribution Function:**
```
P(M ≤ k) = (k/n)²
```

**Expectation (Mean):**
```
E[M] = Σ(k=1 to n) k · P(M = k)
     = Σ(k=1 to n) k · (2k - 1)/n²
     = (2n² + 3n + 1)/(3n)
     ≈ (2n + 3)/3  for large n
```

**Variance:**
```
First, E[M²] = Σ(k=1 to n) k² · (2k - 1)/n²
             = (n + 1)(3n² + 5n - 2)/(6n)

Var(M) = E[M²] - (E[M])²
       = (n + 1)(3n² + 5n - 2)/(6n) - [(2n² + 3n + 1)/(3n)]²
       = (n² - 1)/18
```

**Standard Deviation:**
```
σ(M) = √[(n² - 1)/18]
     ≈ n/(3√2) for large n
```

### Specific Values for d8 (n = 8)

**Expectation:**
```
E[M] = (2·64 + 3·8 + 1)/(3·8)
     = (128 + 24 + 1)/24
     = 153/24
     = 6.375
```

**Variance:**
```
Var(M) = (64 - 1)/18
       = 63/18
       = 3.5
```

**Standard Deviation:**
```
σ(M) = √3.5 ≈ 1.871
```

**Mode:**
The mode is at k = 8 (the maximum value), where P(M = 8) = 15/64 ≈ 0.234

**Probability Distribution for d8:**
```
k    P(M = k)
1    1/64  = 0.0156
2    3/64  = 0.0469
3    5/64  = 0.0781
4    7/64  = 0.1094
5    9/64  = 0.1406
6    11/64 = 0.1719
7    13/64 = 0.2031
8    15/64 = 0.2344
```

Now let me create an interactive visualization:I've created an interactive visualization that shows:

1. **The probability distribution** for M = max(d₁(n), d₂(n)) with adjustable die size (d4 to d20)
2. **Key statistics** displayed prominently: mean, standard deviation, mode, and variance
3. **Visual indicators** on the graph:
   - Green dashed line for the mean (μ)
   - Orange dashed lines for ±1 standard deviation
   - Purple bar highlighting the mode (most likely value)
4. **Complete formulas** for the distribution
5. **Detailed probability table** with exact fractions, decimals, percentages, and cumulative probabilities

## Key Insights for d8:

- **Mean**: 6.375 (the average maximum value)
- **Standard Deviation**: 1.871 
- **Mode**: 8 (23.44% chance - most likely outcome)
- **Variance**: 3.5

The distribution shows that when rolling two d8s and taking the maximum, you're most likely to get 8, and the values cluster toward the higher end of the range. This makes sense intuitively - you only get a low value if *both* dice roll low.

You can use the slider to explore other die types and see how the distribution changes!
