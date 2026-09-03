// ai_strat_ismctsnn_net.c
// See ai_strat_ismctsnn_net.h for the full design rationale.

#include <math.h>
#include <stdio.h>

#include "ai_strat_ismctsnn_net.h"

#define IN_DIM ISMCTSNN_STATE_DIM
#define H1 ISMCTSNN_NET_HIDDEN1
#define H2 ISMCTSNN_NET_HIDDEN2
#define H3 ISMCTSNN_NET_HIDDEN3

typedef struct
{ float w1[H1][IN_DIM];
  float b1[H1];
  float w2[H2][H1];
  float b2[H2];
  float w3[H3][H2];
  float b3[H3];
  float w4[1][H3];
  float b4[1];
} ISMCTSNNWeights;

_Static_assert(sizeof(ISMCTSNNWeights) ==
               (size_t)(H1 * IN_DIM + H1 + H2 * H1 + H2 + H3 * H2 + H3 + H3 + 1) * sizeof(float),
               "ISMCTSNNWeights must be a flat array with no struct padding");

static ISMCTSNNWeights g_weights;
static bool g_loaded = false;

bool ismctsnn_net_load(const char* path)
{ FILE* f = fopen(path, "rb");
  if(!f) return false;

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  if(size != (long)sizeof(ISMCTSNNWeights))
  { fclose(f);
    return false;
  }

  bool ok = (fread(&g_weights, sizeof(ISMCTSNNWeights), 1, f) == 1);
  fclose(f);
  g_loaded = ok;
  return g_loaded;
} // ismctsnn_net_load

bool ismctsnn_net_is_loaded(void)
{ return g_loaded;
} // ismctsnn_net_is_loaded

static float relu(float x)
{ return (x > 0.0f) ? x : 0.0f;
} // relu

// out[i] = relu(b[i] + sum_j w[i][j] * in[j]) -- w is out_dim rows of
// in_dim floats each (row-major, matching PyTorch's nn.Linear.weight
// layout, which export_weights.py writes unchanged).
static void linear_relu(const float* in, uint32_t in_dim, const float* w, const float* b,
                        float* out, uint32_t out_dim)
{ for(uint32_t i = 0; i < out_dim; i++)
  { float acc = b[i];
    const float* wrow = w + (size_t)i * in_dim;
    for(uint32_t j = 0; j < in_dim; j++)
      acc += wrow[j] * in[j];
    out[i] = relu(acc);
  }
} // linear_relu

float ismctsnn_net_forward(const ISMCTSNNStateVector* state)
{ if(!g_loaded) return 0.5f;

  const float* x = (const float*)state;
  float h1[H1], h2[H2], h3[H3];

  linear_relu(x, IN_DIM, &g_weights.w1[0][0], g_weights.b1, h1, H1);
  linear_relu(h1, H1, &g_weights.w2[0][0], g_weights.b2, h2, H2);
  linear_relu(h2, H2, &g_weights.w3[0][0], g_weights.b3, h3, H3);

  float acc = g_weights.b4[0];
  for(uint32_t j = 0; j < H3; j++)
    acc += g_weights.w4[0][j] * h3[j];

  return 1.0f / (1.0f + expf(-acc));
} // ismctsnn_net_forward

float ismctsnn_net_value(const struct gamestate* gstate, PlayerID observer)
{ ISMCTSNNStateVector vec;
  ismctsnn_encode_state(gstate, observer, &vec);
  return ismctsnn_net_forward(&vec);
} // ismctsnn_net_value
