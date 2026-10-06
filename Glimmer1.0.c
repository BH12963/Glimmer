/*
    Glimmer 1.0 - a UCI chess engine written in C17
    Copyright (c) 2026 Bui Hoang Bach

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef unsigned long long U;
typedef unsigned char u8;
typedef uint16_t Move;

#define BIT(s) (1ULL << (s))
#define LSB(x) __builtin_ctzll(x)
#define POP(x) __builtin_popcountll(x)
#define FROM(m) ((m) & 63)
#define TO(m) (((m) >> 6) & 63)
#define PROMO(m) (((m) >> 12) & 7)
#define MK(f, t, p) ((Move)((f) | ((t) << 6) | ((p) << 12)))

#define INF 30000
#define MATE 29000
#define MAXP 100

enum { P, N, B, R, Q, K, E };
enum { WHITE, BLACK };

typedef struct {
  U pt[6], oc[2], key;
  u8 sq[64];
  int stm, ep, cs, hm;
} Bd;

typedef struct {
  unsigned k;
  short s;
  unsigned short m;
  signed char d;
  u8 f; /* flag + age */
} TE;

/* ---------- globals ---------- */
static U Z[2][6][64], ZE[64], ZC[16], ZS;
static U NA[64], KA[64], PA[2][64], LM[3][64], FM[8], ADJ[8];
static U PM[2][64], KS1[2][64], KS2[2][64];
static u8 RT[8][64], CM[64];
static int LR[64][64];

#include "params.h"
static int W[2][NP];

#ifdef TUNE
static int nf, fi[512], fc[512], evph, evstm;
#define F(i, c)                                                                \
  do {                                                                         \
    int _c = (c);                                                              \
    if (_c) {                                                                  \
      fi[nf] = (i);                                                            \
      fc[nf++] = _c;                                                           \
    }                                                                          \
  } while (0)
#else
#define F(i, c)                                                                \
  do {                                                                         \
    int _c = (c);                                                              \
    mg += W[0][i] * _c;                                                        \
    eg += W[1][i] * _c;                                                        \
  } while (0)
#endif

/* ---------- attacks ---------- */
static U sl(U occ, int s, int k) {
  U m = LM[k][s], f = occ & m, r = __builtin_bswap64(f);
  f -= BIT(s);
  r -= __builtin_bswap64(BIT(s));
  f ^= __builtin_bswap64(r);
  return f & m;
}
static U rook(int s, U o) {
  return sl(o, s, 0) | ((U)RT[s & 7][(o >> ((s & 56) + 1)) & 63] << (s & 56));
}
static U bish(int s, U o) { return sl(o, s, 1) | sl(o, s, 2); }

static U atk_to(Bd *b, int s, U o) {
  return (NA[s] & b->pt[N]) | (KA[s] & b->pt[K]) |
         (PA[BLACK][s] & b->pt[P] & b->oc[WHITE]) |
         (PA[WHITE][s] & b->pt[P] & b->oc[BLACK]) |
         (bish(s, o) & (b->pt[B] | b->pt[Q])) |
         (rook(s, o) & (b->pt[R] | b->pt[Q]));
}

static int att(Bd *b, int s, int by) {
  U o = b->oc[0] | b->oc[1], m = b->oc[by];
  return (NA[s] & b->pt[N] & m) || (KA[s] & b->pt[K] & m) ||
         (PA[by ^ 1][s] & b->pt[P] & m) ||
         (bish(s, o) & (b->pt[B] | b->pt[Q]) & m) ||
         (rook(s, o) & (b->pt[R] | b->pt[Q]) & m);
}

/* ---------- init ---------- */
static void init(void) {
  U s = 0x9E3779B97F4A7C15ULL;
#define RND (s ^= s << 13, s ^= s >> 7, s ^= s << 17, s)
  int c, p, i, j, f, r, k;
  for (c = 0; c < 2; c++)
    for (p = 0; p < 6; p++)
      for (i = 0; i < 64; i++)
        Z[c][p][i] = RND;
  for (i = 1; i < 64; i++)
    ZE[i] = RND;
  for (i = 1; i < 16; i++)
    ZC[i] = RND;
  ZS = RND;

  static const int nd[8][2] = {{1, 2},  {2, 1},  {-1, 2},  {-2, 1},
                               {1, -2}, {2, -1}, {-1, -2}, {-2, -1}};
  for (i = 0; i < 64; i++) {
    f = i & 7;
    r = i >> 3;
    for (k = 0; k < 8; k++) {
      int rr = r + nd[k][0], ff = f + nd[k][1];
      if (rr >= 0 && rr < 8 && ff >= 0 && ff < 8)
        NA[i] |= BIT(rr * 8 + ff);
    }
    for (j = -1; j <= 1; j++)
      for (k = -1; k <= 1; k++) {
        int rr = r + j, ff = f + k;
        if ((j || k) && rr >= 0 && rr < 8 && ff >= 0 && ff < 8)
          KA[i] |= BIT(rr * 8 + ff);
      }
    for (k = -1; k <= 1; k += 2) {
      if (f + k >= 0 && f + k < 8) {
        if (r < 7)
          PA[WHITE][i] |= BIT((r + 1) * 8 + f + k);
        if (r > 0)
          PA[BLACK][i] |= BIT((r - 1) * 8 + f + k);
      }
    }
    for (j = 0; j < 64; j++) {
      int rr = j >> 3, ff = j & 7;
      if (j == i)
        continue;
      if (ff == f)
        LM[0][i] |= BIT(j);
      if (rr - ff == r - f)
        LM[1][i] |= BIT(j);
      if (rr + ff == r + f)
        LM[2][i] |= BIT(j);
    }
    for (c = 0; c < 2; c++)
      for (j = 0; j < 64; j++) {
        int rr = j >> 3, ff = j & 7, d = ff - f;
        if (c == WHITE ? rr > r : rr < r) {
          if (d >= -1 && d <= 1) {
            PM[c][i] |= BIT(j);
            if (c == WHITE ? rr == r + 1 : rr == r - 1)
              KS1[c][i] |= BIT(j);
            if (c == WHITE ? rr == r + 2 : rr == r - 2)
              KS2[c][i] |= BIT(j);
          }
        }
      }
  }
  for (f = 0; f < 8; f++) {
    FM[f] = 0x0101010101010101ULL << f;
    ADJ[f] = (f > 0 ? FM[f - 1] : 0) | (f < 7 ? FM[f + 1] : 0);
  }
  for (f = 0; f < 8; f++)
    for (i = 0; i < 64; i++) {
      int o = i << 1, a = 0;
      for (k = f + 1; k < 8; k++) {
        a |= 1 << k;
        if (o & (1 << k))
          break;
      }
      for (k = f - 1; k >= 0; k--) {
        a |= 1 << k;
        if (o & (1 << k))
          break;
      }
      RT[f][i] = a;
    }
  memset(CM, 15, 64);
  CM[0] = 13;
  CM[7] = 14;
  CM[4] = 12;
  CM[56] = 7;
  CM[63] = 11;
  CM[60] = 3;

  for (j = 0; j < 2; j++)
    for (i = 0; i < NP; i++)
      W[j][i] = WI[j][i];
  for (i = 1; i < 64; i++)
    for (j = 1; j < 64; j++)
      LR[i][j] = (int)(0.75 + log(i) * log(j) / 2.25);
}

/* ---------- board ---------- */
#define PUT(c, p, s)                                                           \
  (b->pt[p] |= BIT(s), b->oc[c] |= BIT(s), b->sq[s] = p, b->key ^= Z[c][p][s])
#define DEL(c, p, s)                                                           \
  (b->pt[p] &= ~BIT(s), b->oc[c] &= ~BIT(s), b->sq[s] = E, b->key ^= Z[c][p][s])

static int make(Bd *b, Move m) {
  int from = FROM(m), to = TO(m), pr = PROMO(m);
  int us = b->stm, them = us ^ 1, pt = b->sq[from], cp = b->sq[to], oe = b->ep;

  b->key ^= ZE[oe] ^ ZC[b->cs];
  b->ep = 0;
  b->hm++;

  if (cp < E) {
    DEL(them, cp, to);
    b->hm = 0;
  }
  DEL(us, pt, from);

  if (pt == P) {
    b->hm = 0;
    if (to == oe && oe && (from & 7) != (to & 7))
      DEL(them, P, to ^ 8);
    else if (to - from == 16 || from - to == 16)
      b->ep = (from + to) >> 1;
    PUT(us, pr ? pr : P, to);
  } else
    PUT(us, pt, to);

  if (pt == K && (to - from == 2 || from - to == 2)) {
    int rf = to > from ? to + 1 : to - 2;
    int rt = to > from ? to - 1 : to + 1;
    DEL(us, R, rf);
    PUT(us, R, rt);
  }
  b->cs &= CM[from] & CM[to];
  b->key ^= ZE[b->ep] ^ ZC[b->cs] ^ ZS;
  b->stm = them;
  return !att(b, LSB(b->pt[K] & b->oc[us]), them);
}

static const char *START =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

static void setfen(Bd *b, const char *s) {
  memset(b, 0, sizeof *b);
  memset(b->sq, E, 64);
  int r = 7, f = 0;
  for (; *s && *s != ' '; s++) {
    if (*s == '/') {
      r--;
      f = 0;
    } else if (*s >= '1' && *s <= '8')
      f += *s - '0';
    else {
      const char *q = strchr("pnbrqk", *s | 32);
      int c = *s < 'a';
      int p = q - "pnbrqk";
      PUT(c ? WHITE : BLACK, p, r * 8 + f);
      f++;
    }
  }
  s++;
  b->stm = (*s == 'b');
  s += 2;
  for (; *s && *s != ' '; s++)
    b->cs |= *s == 'K' ? 1 : *s == 'Q' ? 2 : *s == 'k' ? 4 : *s == 'q' ? 8 : 0;
  if (*s)
    s++;
  if (*s && *s != '-') {
    b->ep = (s[1] - '1') * 8 + (s[0] - 'a');
    s += 2;
  } else if (*s)
    s++;
  if (*s)
    b->hm = atoi(s + 1);
  b->key ^= ZC[b->cs] ^ ZE[b->ep] ^ (b->stm ? ZS : 0);
}

/* ---------- move gen ---------- */
static int gen(Bd *b, Move *list, int qs) {
  int n = 0, us = b->stm, them = us ^ 1;
  U occ = b->oc[0] | b->oc[1], mi = b->oc[us], op = b->oc[them];
  U tg = qs ? op : ~mi;
  U x, y;

  /* pawns */
  x = b->pt[P] & mi;
  while (x) {
    int from = LSB(x);
    x &= x - 1;
    int r = from >> 3, up = us ? -8 : 8;
    int p7 = us ? (r == 1) : (r == 6);
    int st = us ? (r == 6) : (r == 1);
    int to = from + up;

    if (!(occ & BIT(to))) {
      if (p7) {
        list[n++] = MK(from, to, Q);
        if (!qs) {
          list[n++] = MK(from, to, N);
          list[n++] = MK(from, to, B);
          list[n++] = MK(from, to, R);
        }
      } else if (!qs) {
        list[n++] = MK(from, to, 0);
        if (st && !(occ & BIT(to + up)))
          list[n++] = MK(from, to + up, 0);
      }
    }
    y = PA[us][from] & (op | (b->ep ? BIT(b->ep) : 0));
    while (y) {
      to = LSB(y);
      y &= y - 1;
      if (p7) {
        list[n++] = MK(from, to, Q);
        if (!qs) {
          list[n++] = MK(from, to, N);
          list[n++] = MK(from, to, B);
          list[n++] = MK(from, to, R);
        }
      } else
        list[n++] = MK(from, to, 0);
    }
  }

  /* pieces */
  for (int pt = N; pt <= Q; pt++) {
    x = b->pt[pt] & mi;
    while (x) {
      int from = LSB(x);
      x &= x - 1;
      U a = pt == N   ? NA[from]
            : pt == B ? bish(from, occ)
            : pt == R ? rook(from, occ)
                      : bish(from, occ) | rook(from, occ);
      a &= tg;
      while (a) {
        int to = LSB(a);
        a &= a - 1;
        list[n++] = MK(from, to, 0);
      }
    }
  }

  /* king */
  int ksq = LSB(b->pt[K] & mi);
  y = KA[ksq] & tg;
  while (y) {
    int to = LSB(y);
    y &= y - 1;
    list[n++] = MK(ksq, to, 0);
  }

  /* castling */
  if (!qs) {
    int c = b->cs >> (us * 2), h = us * 56;
    if ((c & 1) && !(occ & (0x60ULL << h)) && !att(b, 4 + h, them) &&
        !att(b, 5 + h, them))
      list[n++] = MK(4 + h, 6 + h, 0);
    if ((c & 2) && !(occ & (0x0EULL << h)) && !att(b, 4 + h, them) &&
        !att(b, 3 + h, them))
      list[n++] = MK(4 + h, 2 + h, 0);
  }
  return n;
}

/* ---------- SEE ---------- */
static const int V[7] = {100, 320, 330, 500, 900, 20000, 0};

static int see(Bd *b, Move m) {
  int from = FROM(m), to = TO(m), d = 0, g[34];
  int pt = b->sq[from], us = b->stm;
  U occ = b->oc[0] | b->oc[1], fb = BIT(from), at, s;

  g[0] = V[b->sq[to]];
  if (pt == P && to == b->ep && b->sq[to] == E)
    g[0] = 100;

  for (;;) {
    d++;
    g[d] = V[pt] - g[d - 1];
    if (-g[d - 1] < 0 && g[d] < 0)
      break;
    occ ^= fb;
    at = atk_to(b, to, occ) & occ;
    us ^= 1;
    s = at & b->oc[us];
    if (!s)
      break;
    for (pt = 0; !(s & b->pt[pt]); pt++)
      ;
    fb = s & b->pt[pt];
    fb &= -fb;
  }
  while (--d)
    g[d - 1] = -(-g[d - 1] > g[d] ? -g[d - 1] : g[d]);
  return g[0];
}

/* ---------- eval ---------- */
static int eval(Bd *b) {
  static const int PW[6] = {0, 1, 1, 2, 4, 0}, KWT[6] = {0, 2, 2, 3, 5, 0};
  static const int MO[4] = {MOBN, MOBB, MOBR, MOBQ};
  int mg = 0, eg = 0, ph = 0, c, pt;
  int ka[2] = {0}, kn[2] = {0};
  U occ = b->oc[0] | b->oc[1];
  U pwn[2] = {b->pt[P] & b->oc[WHITE], b->pt[P] & b->oc[BLACK]};
  U at[2][5] = {{0}};
  U pa[2] = {((pwn[WHITE] << 7) & ~0x8080808080808080ULL) |
                 ((pwn[WHITE] << 9) & ~0x0101010101010101ULL),
             ((pwn[BLACK] >> 9) & ~0x8080808080808080ULL) |
                 ((pwn[BLACK] >> 7) & ~0x0101010101010101ULL)};
  U all[2];

  for (c = 0; c < 2; c++) {
    int sg = c ? -1 : 1;
    U mine = b->oc[c], pw = pwn[c], epw = pwn[c ^ 1];
    int ek = LSB(b->pt[K] & b->oc[c ^ 1]);
    int mk = LSB(b->pt[K] & mine);
    int kf = mk & 7;
    U kz = KA[ek] | BIT(ek);
    U ma = ~(pw | pa[c ^ 1] | (b->pt[K] & mine));

    for (pt = 0; pt < 6; pt++) {
      U x = b->pt[pt] & mine;
      while (x) {
        int s = LSB(x);
        x &= x - 1;
        int r = c ? s ^ 56 : s;
        int f = s & 7, rr = c ? 7 - (s >> 3) : s >> 3;
        ph += PW[pt];
        F(pt * 64 + r, sg);

        if (pt > P && pt < K) {
          U a = pt == N   ? NA[s]
                : pt == B ? bish(s, occ)
                : pt == R ? rook(s, occ)
                          : bish(s, occ) | rook(s, occ);
          at[c][pt] |= a;
          F(MO[pt - 1] + POP(a & ma), sg);
          if (a & kz) {
            ka[c] += KWT[pt];
            kn[c]++;
          }
          if (pt == R) {
            U fm = FM[f];
            if (!(fm & pw))
              F(!(fm & epw) ? ROPEN : RSEMI, sg);
            if (rr == 6)
              F(R7, sg);
          } else if (pt == N && rr >= 3 && rr <= 5 && (BIT(s) & pa[c]) &&
                     !(PM[c][s] & ADJ[f] & epw))
            F(OUTP, sg);
        } else if (pt == P) {
          if (!(PM[c][s] & epw)) {
            F(PASS + rr, sg);
            if (!(occ & BIT(c ? s - 8 : s + 8)))
              F(PFREE + rr, sg);
          }
          if (!(ADJ[f] & pw))
            F(ISO, sg);
          if (PM[c][s] & FM[f] & pw)
            F(DBL, sg);
          if (BIT(s) & pa[c])
            F(SUP, sg);
          if ((((BIT(s) << 1) & ~0x0101010101010101ULL) |
               ((BIT(s) >> 1) & ~0x8080808080808080ULL)) &
              pw)
            F(PHAL, sg);
        }
      }
    }
    if (POP(b->pt[B] & mine) > 1)
      F(BPAIR, sg);
    F(SH1, sg * POP(pw & KS1[c][mk]));
    F(SH2, sg * POP(pw & KS2[c][mk]));
    for (int i = kf > 0 ? kf - 1 : 0; i <= (kf < 7 ? kf + 1 : 7); i++)
      if (!(FM[i] & pw))
        F(KOPEN, sg);
    all[c] = pa[c] | KA[mk] | at[c][N] | at[c][B] | at[c][R] | at[c][Q];
  }

  for (c = 0; c < 2; c++) {
    int sg = c ? 1 : -1, e = c ^ 1;
    U my = b->oc[c], mi = at[e][N] | at[e][B];
    for (pt = N; pt <= Q; pt++)
      F(THRP + pt - 1, sg * POP(b->pt[pt] & my & pa[e]));
    F(THRMR, sg * POP(b->pt[R] & my & mi));
    F(THRMQ, sg * POP(b->pt[Q] & my & (mi | at[e][R])));
    F(HANG, sg * POP(my & ~b->pt[P] & ~b->pt[K] & all[e] & ~all[c]));
    F(KATT + (kn[e] > 1 ? (ka[e] > 25 ? 25 : ka[e]) : 0), sg);
  }
  F(TEMPO, b->stm ? -1 : 1);
  if (ph > 24)
    ph = 24;

#ifdef TUNE
  evph = ph;
  evstm = b->stm;
  return 0;
#else
  int sc = (mg * ph + eg * (24 - ph)) / 24;
  return b->stm ? -sc : sc;
#endif
}

/* ---------- search ---------- */
static TE *tt;
static U TM;
static int age;
static U hk[2048];
static int R0;
static int hist[2][64][64], kl[MAXP + 4][2], ev[MAXP + 4];
static long nodes, nlim;
static int ab, rb;
static double t0, soft, hard;

static double ms(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}
static void chk(void) {
  if ((nlim && nodes >= nlim) || ms() - t0 >= hard)
    ab = 1;
}

static void score(Bd *b, Move *list, int *sc, int n, Move ttm, int ply) {
  for (int i = 0; i < n; i++) {
    Move m = list[i];
    int from = FROM(m), to = TO(m), pt = b->sq[from], v = b->sq[to],
        pr = PROMO(m);
    int cap = v < E || (pt == P && to == b->ep && b->ep);
    int s;
    if (m == ttm)
      s = 100000000;
    else if (cap) {
      int vv = v < E ? V[v] : 100;
      s = (vv >= V[pt] || see(b, m) >= 0) ? 2000000 : -1000000;
      s += vv * 8 - pt;
      if (pr == Q)
        s += 100000;
      else if (pr)
        s = -3000000;
    } else if (pr)
      s = pr == Q ? 1900000 : -3000000;
    else if (m == kl[ply][0])
      s = 1500000;
    else if (m == kl[ply][1])
      s = 1400000;
    else
      s = hist[b->stm][from][to];
    sc[i] = s;
  }
}

static int qs(Bd *b, int a, int bt, int ply) {
  int inchk, best = -INF, sp = 0, n, i, v, legal = 0;
  Move list[256];
  int sc[256];

  if (!(++nodes & 1023))
    chk();
  if (ab)
    return 0;
  if (ply >= MAXP)
    return eval(b);

  inchk = att(b, LSB(b->pt[K] & b->oc[b->stm]), b->stm ^ 1);
  if (!inchk) {
    best = sp = eval(b);
    if (best >= bt)
      return best;
    if (best > a)
      a = best;
  }

  n = gen(b, list, !inchk);
  score(b, list, sc, n, 0, ply);

  for (i = 0; i < n; i++) {
    int j = i, k;
    for (k = i + 1; k < n; k++)
      if (sc[k] > sc[j])
        j = k;
    Move m = list[j];
    list[j] = list[i];
    list[i] = m;
    k = sc[j];
    sc[j] = sc[i];
    sc[i] = k;

    if (!inchk) {
      if (sc[i] < 0)
        continue;
      int cv = b->sq[TO(m)] < E ? V[b->sq[TO(m)]] : 100;
      if (!PROMO(m) && sp + cv + 200 < a)
        continue;
    }

    Bd nb = *b;
    if (!make(&nb, m))
      continue;
    legal++;
    v = -qs(&nb, -bt, -a, ply + 1);
    if (ab)
      return 0;
    if (v > best) {
      best = v;
      if (v > a) {
        a = v;
        if (a >= bt)
          break;
      }
    }
  }
  if (inchk && !legal)
    return -MATE + ply;
  return best;
}

static int srch(Bd *b, int d, int a, int bt, int ply, int nm) {
  int pv = bt - a > 1, inchk, v, best = -INF, bm = 0, played = 0, se = -INF,
      imp = 0;
  int i, n, nq = 0, oa = a;
  Move list[256], q[64];
  int sc[256];

  if (ply) {
    if (b->hm >= 100)
      return 0;
    for (i = ply + R0 - 2; i >= 0 && i >= ply + R0 - b->hm; i -= 2)
      if (hk[i] == b->key)
        return 0;
    if (!(b->pt[P] | b->pt[R] | b->pt[Q]) && POP(b->pt[N] | b->pt[B]) < 2)
      return 0;
    if (a < -MATE + ply)
      a = -MATE + ply;
    if (bt > MATE - ply - 1)
      bt = MATE - ply - 1;
    if (a >= bt)
      return a;
  }

  hk[R0 + ply] = b->key;
  inchk = att(b, LSB(b->pt[K] & b->oc[b->stm]), b->stm ^ 1);
  if (inchk)
    d++;
  if (d <= 0)
    return qs(b, a, bt, ply);
  if (ply >= MAXP - 1)
    return eval(b);
  if (!(++nodes & 1023))
    chk();
  if (ab)
    return 0;

  TE *e = &tt[b->key & TM & ~1ULL];
  unsigned ck = b->key >> 32;
  Move ttm = 0;
  if (e[0].k != ck && e[1].k == ck)
    e++;
  if (e->k == ck) {
    ttm = e->m;
    if (!pv && e->d >= d) {
      int s = e->s, fl = e->f & 3;
      if (s > MATE - MAXP)
        s -= ply;
      else if (s < -MATE + MAXP)
        s += ply;
      if (fl == 1 || (fl == 2 && s >= bt) || (fl == 3 && s <= a))
        return s;
    }
  }

  if (!inchk)
    se = eval(b);
  ev[ply] = se;
  imp = ply >= 2 && !inchk && ev[ply - 2] > -INF && se > ev[ply - 2];

  if (!pv && !inchk && ply) {
    if (d <= 6 && se - 80 * (d - imp) >= bt)
      return se;
    if (nm && d >= 3 && se >= bt && (b->oc[b->stm] & ~(b->pt[P] | b->pt[K]))) {
      Bd nb = *b;
      nb.key ^= ZE[nb.ep] ^ ZS;
      nb.ep = 0;
      nb.stm ^= 1;
      nb.hm++;
      int r = 3 + d / 3 + (se - bt > 600 ? 3 : (se - bt) / 200);
      v = -srch(&nb, d - 1 - r, -bt, -bt + 1, ply + 1, 0);
      if (ab)
        return 0;
      if (v >= bt)
        return v >= MATE - MAXP ? bt : v;
    }
  }

  if (!ttm && d >= 4)
    d--;
  n = gen(b, list, 0);
  score(b, list, sc, n, ttm, ply);

  for (i = 0; i < n; i++) {
    int j = i, k;
    for (k = i + 1; k < n; k++)
      if (sc[k] > sc[j])
        j = k;
    Move m = list[j];
    list[j] = list[i];
    list[i] = m;
    k = sc[j];
    sc[j] = sc[i];
    sc[i] = k;

    int from = FROM(m), to = TO(m), pc = b->sq[from], pr = PROMO(m);
    int cap = b->sq[to] < E || (pc == P && to == b->ep && b->ep);
    int quiet = !cap && !pr;

    Bd nb = *b;
    if (!make(&nb, m))
      continue;

    int gc = att(&nb, LSB(nb.pt[K] & nb.oc[nb.stm]), nb.stm ^ 1);

    if (!pv && !inchk && best > -MATE + MAXP && played > 0 && !gc) {
      if (quiet) {
        if (d <= 8 && played >= (3 + d * d) / (2 - imp))
          continue;
        if (d <= 6 && se + 100 + 80 * d <= a)
          continue;
        if (d <= 6 && see(b, m) < -25 * d * d)
          continue;
      } else if (cap && !pr && d <= 4 && sc[i] < 0)
        continue;
    }

    played++;
    if (quiet && nq < 64)
      q[nq++] = m;

    int r = 0;
    if (d >= 3 && played > 1 && (quiet || (cap && sc[i] < 0))) {
      r = LR[d < 64 ? d : 63][played < 64 ? played : 63];
      if (!pv)
        r++;
      if (imp)
        r--;
      if (gc || inchk)
        r--;
      if (quiet)
        r -= hist[b->stm][from][to] / 5000;
      if (r < 0)
        r = 0;
      if (r > d - 2)
        r = d - 2;
    }

    if (played == 1)
      v = -srch(&nb, d - 1, -bt, -a, ply + 1, 1);
    else {
      v = -srch(&nb, d - 1 - r, -a - 1, -a, ply + 1, 1);
      if (v > a && r)
        v = -srch(&nb, d - 1, -a - 1, -a, ply + 1, 1);
      if (v > a && v < bt)
        v = -srch(&nb, d - 1, -bt, -a, ply + 1, 1);
    }
    if (ab)
      return 0;

    if (v > best) {
      best = v;
      bm = m;
      if (v > a) {
        a = v;
        if (!ply)
          rb = m;
        if (a >= bt) {
          if (quiet) {
            if (kl[ply][0] != m) {
              kl[ply][1] = kl[ply][0];
              kl[ply][0] = m;
            }
            int bn = d * d > 400 ? 400 : d * d;
            int us = b->stm;
            for (k = 0; k < nq; k++) {
              Move mm = q[k];
              int *h = &hist[us][FROM(mm)][TO(mm)];
              int g = (mm == m) ? bn : -bn;
              *h += g - *h * (g < 0 ? -g : g) / 16384;
            }
          }
          break;
        }
      }
    }
  }

  if (!played)
    return inchk ? -MATE + ply : 0;

  {
    int s = best;
    if (s > MATE - MAXP)
      s += ply;
    else if (s < -MATE + MAXP)
      s -= ply;

    if (e->k != ck) {
      TE *o = &tt[b->key & TM & ~1ULL];
      int s0 = o[0].d - 8 * ((age - (o[0].f >> 2)) & 63);
      int s1 = o[1].d - 8 * ((age - (o[1].f >> 2)) & 63);
      e = s1 < s0 ? &o[1] : &o[0];
    } else if (!bm)
      bm = e->m;

    e->k = ck;
    e->s = s;
    e->m = bm;
    e->d = d;
    e->f = (best >= bt ? 2 : best > oa ? 1 : 3) | (age << 2);
  }
  return best;
}

/* ---------- UCI ---------- */
static Bd G;
static int hn;

static void mstr(Move m, char *s) {
  s[0] = 'a' + (FROM(m) & 7);
  s[1] = '1' + (FROM(m) >> 3);
  s[2] = 'a' + (TO(m) & 7);
  s[3] = '1' + (TO(m) >> 3);
  int p = PROMO(m);
  s[4] = p ? "?nbrq"[p] : 0;
  s[5] = 0;
  if (!p)
    s[4] = 0;
}

static int arg(const char *l, const char *k, int def) {
  const char *p = strstr(l, k);
  return p ? atoi(p + strlen(k)) : def;
}

static int think(int md, int verbose, int *score) {
  Move list[256], legal[256];
  int n = gen(&G, list, 0), nl = 0, i, sc = 0, d, pbm = 0, stab = 0;

  for (i = 0; i < n; i++) {
    Bd nb = G;
    if (make(&nb, list[i]))
      legal[nl++] = list[i];
  }
  rb = nl ? legal[0] : 0;
  memset(kl, 0, sizeof kl);
  age = (age + 1) & 63;

  double lim = soft;
  if (nl > 1)
    for (d = 1; d <= md && d < MAXP - 4; d++) {
      int a = -INF, bt = INF, w = 20, v;
      if (d >= 5) {
        a = sc - w;
        bt = sc + w;
      }
      for (;;) {
        v = srch(&G, d, a, bt, 0, 0);
        if (ab)
          break;
        if (v <= a) {
          a = v - w < -INF ? -INF : v - w;
          w *= 2;
          if (w > 400)
            a = -INF;
          if (d >= 6)
            lim = soft * 1.6;
        } else if (v >= bt) {
          bt = v + w > INF ? INF : v + w;
          w *= 2;
          if (w > 400)
            bt = INF;
        } else
          break;
      }
      if (ab)
        break;
      if (d >= 6 && v < sc - 30)
        lim = soft * 1.6;
      sc = v;
      if (rb == pbm)
        stab++;
      else {
        stab = 0;
        pbm = rb;
      }
      if (verbose)
        printf("info depth %d score cp %d nodes %ld time %d\n", d, sc, nodes,
               (int)(ms() - t0));
      double el = ms() - t0;
      if (el > lim * (stab >= 4 ? 0.35 : stab >= 2 ? 0.5 : 0.7))
        break;
      if (d >= 8 && (sc > MATE - MAXP || sc < -MATE - MAXP))
        break;
    }
  if (score)
    *score = sc;
  return rb;
}

static void go(char *ln) {
  int wt = arg(ln, "wtime ", -1), bt = arg(ln, "btime ", -1);
  int wi = arg(ln, "winc ", 0), bi = arg(ln, "binc ", 0);
  int mtg = arg(ln, "movestogo ", 0), mt = arg(ln, "movetime ", -1);
  int md = arg(ln, "depth ", 64);
  nlim = arg(ln, "nodes ", 0);

  int rem = G.stm ? bt : wt, inc = G.stm ? bi : wi;
  soft = hard = 1e9;
  if (mt > 0)
    soft = hard = mt;
  else if (rem >= 0) {
    int mg = mtg ? mtg : 25;
    soft = (double)rem / mg + inc * 0.75;
    hard = soft * 5;
    if (hard > rem * 0.35)
      hard = rem * 0.35;
    if (soft > hard)
      soft = hard;
    hard -= 15;
    if (hard < 2)
      hard = 2;
  }
  t0 = ms();
  ab = 0;
  nodes = 0;
  R0 = hn - 1;
  char s[8];
  mstr(think(md, 1, 0), s);
  printf("bestmove %s\n", s);
  fflush(stdout);
}

static void pos(char *ln) {
  char *p;
  if ((p = strstr(ln, "startpos")))
    setfen(&G, START);
  else if ((p = strstr(ln, "fen ")))
    setfen(&G, p + 4);
  hn = 0;
  hk[hn++] = G.key;
  p = strstr(ln, "moves");
  if (p) {
    p += 5;
    while (*p) {
      while (*p == ' ')
        p++;
      if (*p < 'a' || *p > 'h')
        break;
      int from = p[0] - 'a' + (p[1] - '1') * 8;
      int to = p[2] - 'a' + (p[3] - '1') * 8;
      int pr = 0, n, i;
      Move list[256];
      if (p[4] >= 'a' && p[4] <= 'z' && p[4] != ' ')
        pr = (int)(strchr("?nbrq", p[4]) - "?nbrq");
      n = gen(&G, list, 0);
      for (i = 0; i < n; i++)
        if (FROM(list[i]) == from && TO(list[i]) == to &&
            PROMO(list[i]) == pr) {
          make(&G, list[i]);
          break;
        }
      hk[hn++] = G.key;
      while (*p && *p != ' ' && *p != '\n')
        p++;
    }
  }
}

static void alloc(int mb) {
  U n = 1;
  while (n * 2 * sizeof(TE) <= (U)mb << 20)
    n *= 2;
  free(tt);
  tt = calloc(n, sizeof(TE));
  TM = n - 1;
}

int main(void) {
  char ln[8192];
  init();
  alloc(64);
  setfen(&G, START);
  hn = 1;
  hk[0] = G.key;
  setvbuf(stdout, 0, _IOLBF, 0);

  while (fgets(ln, sizeof ln, stdin)) {
    if (!strncmp(ln, "uci", 3) && ln[3] == '\n')
      puts("id name Glimmer 1.0\nid author Bach Bui Hoang\n"
           "option name Hash type spin default 64 min 1 max 4096\nuciok");
    else if (!strncmp(ln, "isready", 7))
      puts("readyok");
    else if (!strncmp(ln, "ucinewgame", 10)) {
      memset(tt, 0, (TM + 1) * sizeof(TE));
      memset(hist, 0, sizeof hist);
    } else if (!strncmp(ln, "position", 8))
      pos(ln);
    else if (!strncmp(ln, "go", 2))
      go(ln);
    else if (!strncmp(ln, "setoption", 9)) {
      char *p = strstr(ln, "value ");
      if (p && strstr(ln, "Hash"))
        alloc(atoi(p + 6));
    } else if (!strncmp(ln, "quit", 4))
      break;
  }
  return 0;
}
