// ai_strat_puct_net.c
// See ai_strat_puct_net.h for the full design rationale.

#include <math.h>
#include <stdio.h>

#include "ai_strat_puct_net.h"

#define IN_DIM ISMCTSNN_STATE_DIM
#define H1 PUCT_NET_HIDDEN1
#define H2 PUCT_NET_HIDDEN2
#define H3 PUCT_NET_HIDDEN3
#define POLICY_DIM PUCT_POLICY_DIM

typedef struct
{ float w1[H1][IN_DIM];
  float b1[H1];
  float w2[H2][H1];
  float b2[H2];
  float w3[H3][H2];
  float b3[H3];
  float wv[1][H3];
  float bv[1];
  float wp[POLICY_DIM][H3];
  float bp[POLICY_DIM];
} PUCTNetWeights;

_Static_assert(sizeof(PUCTNetWeights) ==
               (size_t)(H1 * IN_DIM + H1 + H2 * H1 + H2 + H3 * H2 + H3 + H3 + 1
                        + POLICY_DIM * H3 + POLICY_DIM) * sizeof(float),
               "PUCTNetWeights must be a flat array with no struct padding");

static PUCTNetWeights g_weights;
static bool g_loaded = false;

bool puctnet_load(const char* path)
{ FILE* f = fopen(path, "rb");
  if(!f) return false;

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  if(size != (long)sizeof(PUCTNetWeights))
  { fclose(f);
    return false;
  }

  bool ok = (fread(&g_weights, sizeof(PUCTNetWeights), 1, f) == 1);
  fclose(f);
  g_loaded = ok;
  return g_loaded;
} // puctnet_load

bool puctnet_is_loaded(void)
{ return g_loaded;
} // puctnet_is_loaded

static float relu(float x)
{ return (x > 0.0f) ? x : 0.0f;
} // relu

// out[i] = relu(b[i] + sum_j w[i][j] * in[j]) -- w is out_dim rows of
// in_dim floats each (row-major, matching PyTorch's nn.Linear.weight
// layout, which export_puct_weights.py writes unchanged) -- same shape as
// ai_strat_ismctsnn_net.c's own linear_relu(), duplicated rather than
// shared (this project's manual-code-over-macro-magic convention; the two
// modules' weight structs are deliberately disjoint types).
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

// Raw (no activation) linear layer -- the policy head's logits feed
// puct_compose_priors()'s own softmax (ai_strat_puct_policy.c) later, over
// the legal move list this module never sees, not a softmax/sigmoid here.
static void linear(const float* in, uint32_t in_dim, const float* w, const float* b,
                   float* out, uint32_t out_dim)
{ for(uint32_t i = 0; i < out_dim; i++)
  { float acc = b[i];
    const float* wrow = w + (size_t)i * in_dim;
    for(uint32_t j = 0; j < in_dim; j++)
      acc += wrow[j] * in[j];
    out[i] = acc;
  }
} // linear

void puctnet_forward(const ISMCTSNNStateVector* state, float* value_out,
                     float policy_logits_out[PUCT_POLICY_DIM])
{ if(!g_loaded)
  { *value_out = 0.5f;
    for(uint32_t i = 0; i < POLICY_DIM; i++) policy_logits_out[i] = 0.0f;
    return;
  }

  const float* x = (const float*)state;
  float h1[H1], h2[H2], h3[H3];
  linear_relu(x, IN_DIM, &g_weights.w1[0][0], g_weights.b1, h1, H1);
  linear_relu(h1, H1, &g_weights.w2[0][0], g_weights.b2, h2, H2);
  linear_relu(h2, H2, &g_weights.w3[0][0], g_weights.b3, h3, H3);

  float v_acc = g_weights.bv[0];
  for(uint32_t j = 0; j < H3; j++) v_acc += g_weights.wv[0][j] * h3[j];
  *value_out = 1.0f / (1.0f + expf(-v_acc));

  linear(h3, H3, &g_weights.wp[0][0], g_weights.bp, policy_logits_out, POLICY_DIM);
} // puctnet_forward

void puctnet_value_and_policy(const struct gamestate* gstate, PlayerID observer,
                              float* value_out, float policy_logits_out[PUCT_POLICY_DIM])
{ ISMCTSNNStateVector vec;
  ismctsnn_encode_state(gstate, observer, &vec);
  puctnet_forward(&vec, value_out, policy_logits_out);
} // puctnet_value_and_policy
