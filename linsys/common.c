#include "common.h"
/* contains routines common to direct and indirect sparse solvers */

#define MIN_SCALE (1e-3)
#define MAX_SCALE (1e3)
#define NUM_SCALE_PASSES 1 /* additional passes don't help much */

scs_int validateLinSys(const AMatrix * A) {
	scs_int i, rMax, Anz;
	if (!A->x || !A->i || !A->p) {
		scs_printf("data incompletely specified\n");
		return -1;
	}
    /* detects some errors in A col ptrs: */
    for (i = 0; i < A->n; ++i) {
        if (A->p[i] == A->p[i + 1]) {
            scs_printf("WARN: A->p (column pointers) not strictly increasing, column %li empty\n", (long) i);
        } else if (A->p[i] > A->p[i + 1]) {
            scs_printf("ERROR: A->p (column pointers) decreasing\n");
            return -1;
        }
    }
    Anz = A->p[A->n];
    if (((scs_float) Anz / A->m > A->n) || (Anz <= 0)) {
        scs_printf("Anz (nonzeros in A) = %li, outside of valid range\n", (long) Anz);
        return -1;
    }
    rMax = 0;
    for (i = 0; i < Anz; ++i) {
        if (A->i[i] > rMax)
            rMax = A->i[i];
    }
    if (rMax > A->m - 1) {
        scs_printf("number of rows in A inconsistent with input dimension\n");
        return -1;
    }
    return 0;
}

void freeAMatrix(const AMatrix * A) {
    if (A->x)
        scs_free(A->x);
    if (A->i)
        scs_free(A->i);
    if (A->p)
        scs_free(A->p);
}

void printAMatrix(const AMatrix * A) {
	scs_int i, j;
	/* TODO: this is to prevent clogging stdout */
	if (A->p[A->n] < 2500) {
		scs_printf("\n");
		for (i = 0; i < A->n; ++i) {
			scs_printf("Col %li: ", (long) i);
			for (j = A->p[i]; j < A->p[i + 1]; j++) {
				scs_printf("A[%li,%li] = %4f, ", (long) A->i[j], (long) i, A->x[j]);
			}
			scs_printf("norm col = %4f\n", calcNorm(&(A->x[A->p[i]]), A->p[i + 1] - A->p[i]));
		}
		scs_printf("norm A = %4f\n", calcNorm(A->x, A->p[A->n]));
	}
}

void normalizeA(const AMatrix * A, const Settings * stgs, Cone * k, Scaling * scal) {
	/* TODO: fix */
	scs_float * D = scs_malloc(A->m * sizeof(scs_float));
	scs_float * E = scs_malloc(A->n * sizeof(scs_float));
	scs_float * Dt = scs_malloc(A->m * sizeof(scs_float));
	scs_float * Et = scs_malloc(A->n * sizeof(scs_float));
	scs_float * nms = scs_calloc(A->m, sizeof(scs_float));
	scs_float minRowScale = MIN_SCALE * SQRTF((scs_float) A->n), maxRowScale = MAX_SCALE * SQRTF((scs_float) A->n);
	scs_float minColScale = MIN_SCALE * SQRTF((scs_float) A->m), maxColScale = MAX_SCALE * SQRTF((scs_float) A->m);
	scs_int i, j, l, count, delta, *boundaries, c1, c2;
	scs_float wrk, e;
	scs_int numBoundaries = getConeBoundaries(k, &boundaries);

#ifdef EXTRAVERBOSE
	timer normalizeTimer;
	tic(&normalizeTimer);
	scs_printf("normalizing A\n");
	printAMatrix(A);
#endif

	for (l = 0; l < NUM_SCALE_PASSES; ++l) {
		memset(D, 0, A->m * sizeof(scs_float));
		memset(E, 0, A->n * sizeof(scs_float));
		/* calculate row norms */
		for (i = 0; i < A->n; ++i) {
			c1 = A->p[i];
			c2 = A->p[i + 1];
			for (j = c1; j < c2; ++j) {
				wrk = A->x[j];
				D[A->i[j]] += wrk * wrk;
			}
		}
		for (i = 0; i < A->m; ++i) {
			D[i] = SQRTF(D[i]); /* just the norms */
		}

		/* mean of norms of rows across each cone  */
		count = boundaries[0];
		for (i = 1; i < numBoundaries; ++i) {
			wrk = 0;
			delta = boundaries[i];
			for (j = count; j < count + delta; ++j) {
				wrk += D[j];
			}
			wrk /= delta;
			for (j = count; j < count + delta; ++j) {
				D[j] = wrk;
			}
			count += delta;
		}

		for (i = 0; i < A->m; ++i) {
			if (D[i] < minRowScale)
				D[i] = 1;
			else if (D[i] > maxRowScale)
				D[i] = maxRowScale;
		}

		/* scale the rows with D */
		for (i = 0; i < A->n; ++i) {
			for (j = A->p[i]; j < A->p[i + 1]; ++j) {
				A->x[j] /= D[A->i[j]];
			}
		}
		/* calculate and scale by col norms, E */
		for (i = 0; i < A->n; ++i) {
			c1 = A->p[i + 1] - A->p[i];
			e = calcNorm(&(A->x[A->p[i]]), c1);
			if (e < minColScale)
				e = 1;
			else if (e > maxColScale)
				e = maxColScale;
			scaleArray(&(A->x[A->p[i]]), 1.0 / e, c1);
			E[i] = e;
		}

		for (i = 0; i < A->m; ++i) {
			Dt[i] = (l == 0) ? D[i] : Dt[i] * D[i];
		}
		for (i = 0; i < A->n; ++i) {
			Et[i] = (l == 0) ? E[i] : Et[i] * E[i];
		}
	}
	scs_free(boundaries);
	scs_free(D);
	scs_free(E);

	/* calculate mean of row norms of A */
	for (i = 0; i < A->n; ++i) {
		for (j = A->p[i]; j < A->p[i + 1]; ++j) {
			wrk = A->x[j];
			nms[A->i[j]] += wrk * wrk;
		}
	}
	scal->meanNormRowA = 0.0;
	for (i = 0; i < A->m; ++i) {
		scal->meanNormRowA += SQRTF(nms[i]) / A->m;
	}
	scs_free(nms);

	/* calculate mean of col norms of A */
	scal->meanNormColA = 0.0;
	for (i = 0; i < A->n; ++i) {
		c1 = A->p[i + 1] - A->p[i];
		scal->meanNormColA += calcNorm(&(A->x[A->p[i]]), c1) / A->n;
	}

	/* scale up by d->SCALE if not equal to 1 */
	if (stgs->scale != 1) {
		scaleArray(A->x, stgs->scale, A->p[A->n]);
	}

	scal->D = Dt;
	scal->E = Et;

#ifdef EXTRAVERBOSE
	scs_printf("finished normalizing A, time: %1.2es\n", tocq(&normalizeTimer) / 1e3);
	printAMatrix(A);
#endif
}

void unNormalizeA(const AMatrix * A, const Settings * stgs, Scaling * scal) {
	scs_int i, j;
	scs_float * D = scal->D;
	scs_float * E = scal->E;
	for (i = 0; i < A->n; ++i) {
		scaleArray(&(A->x[A->p[i]]), E[i] / stgs->scale, A->p[i + 1] - A->p[i]);
	}
	for (i = 0; i < A->n; ++i) {
		for (j = A->p[i]; j < A->p[i + 1]; ++j) {
			A->x[j] *= D[A->i[j]];
		}
	}
}
