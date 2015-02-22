#ifndef SMOOTHED_AGGREGATION_H
#define SMOOTHED_AGGREGATION_H

#include <iostream>
#include <vector>
#include <iterator>
#include <algorithm>

#include <assert.h>
#include <cmath>

#include "linalg.h"
 

/*
 *  Compute a strength of connection matrix using the standard symmetric
 *  Smoothed Aggregation heuristic.  Both the input and output matrices
 *  are stored in CSR format.  A nonzero connection A[i,j] is considered
 *  strong if:
 *
 *      abs(A[i,j]) >= theta * sqrt( abs(A[i,i]) * abs(A[j,j]) )
 *
 *  The strength of connection matrix S is simply the set of nonzero entries
 *  of A that qualify as strong connections.
 *
 *  Parameters
 *      num_rows   - number of rows in A
 *      theta      - stength of connection tolerance
 *      Ap[]       - CSR row pointer
 *      Aj[]       - CSR index array
 *      Ax[]       - CSR data array
 *      Sp[]       - (output) CSR row pointer
 *      Sj[]       - (output) CSR index array
 *      Sx[]       - (output) CSR data array
 *
 *  
 *  Returns:
 *      Nothing, S will be stored in Sp, Sj, Sx
 *
 *  Notes:
 *      Storage for S must be preallocated.  Since S will consist of a subset
 *      of A's nonzero values, a conservative bound is to allocate the same
 *      storage for S as is used by A.
 *
 */
template<class I, class T, class F>
void symmetric_strength_of_connection(const I n_row, 
                                      const F theta,
                                      const I Ap[], const I Aj[], const T Ax[],
                                            I Sp[],       I Sj[],       T Sx[])
{
    //Sp,Sj form a CSR representation where the i-th row contains
    //the indices of all the strong connections from node i
    std::vector<F> diags(n_row);

    //compute norm of diagonal values
    for(I i = 0; i < n_row; i++){
        T diag = 0.0;
        for(I jj = Ap[i]; jj < Ap[i+1]; jj++){
            if(Aj[jj] == i){
                diag += Ax[jj]; //gracefully handle duplicates
            }
        }    
        diags[i] = mynorm(diag);
    }

    I nnz = 0;
    Sp[0] = 0;

    for(I i = 0; i < n_row; i++){

        F eps_Aii = theta*theta*diags[i];

        for(I jj = Ap[i]; jj < Ap[i+1]; jj++){
            const I   j = Aj[jj];
            const T Aij = Ax[jj];

            if(i == j){
                // Always add the diagonal
                Sj[nnz] =   j;
                Sx[nnz] = Aij;
                nnz++;
            }  
            else if(mynormsq(Aij) >= eps_Aii * diags[j]){    
                //  |A(i,j)| >= theta * sqrt(|A(i,i)|*|A(j,j)|) 
                Sj[nnz] =   j;
                Sx[nnz] = Aij;
                nnz++;
            }
        }
        Sp[i+1] = nnz;
    }
}


/*
 * Compute aggregates for a matrix A stored in CSR format
 *
 * Parameters:
 *   n_row         - number of rows in A
 *   Ap[n_row + 1] - CSR row pointer
 *   Aj[nnz]       - CSR column indices
 *    x[n_row]     - aggregate numbers for each node
 *    y[n_row]     - will hold Cpts upon return
 *
 * Returns:
 *  The number of aggregates (== max(x[:]) + 1 )
 *
 * Notes:
 *    It is assumed that A is symmetric.
 *    A may contain diagonal entries (self loops)
 *    Unaggregated nodes are marked with a -1
 *    
 */
template <class I>
I standard_aggregation(const I n_row,
                       const I Ap[], 
                       const I Aj[],
                             I  x[],
                             I  y[])
{
    // Bj[n] == -1 means i-th node has not been aggregated
    std::fill(x, x + n_row, 0);

    I next_aggregate = 1; // number of aggregates + 1

    //Pass #1
    for(I i = 0; i < n_row; i++){
        if(x[i]){ continue; } //already marked

        const I row_start = Ap[i];
        const I row_end   = Ap[i+1];

        //Determine whether all neighbors of this node are free (not already aggregates)
        bool has_aggregated_neighbors = false;
        bool has_neighbors            = false;
        for(I jj = row_start; jj < row_end; jj++){
            const I j = Aj[jj];
            if( i != j ){
                has_neighbors = true;
                if( x[j] ){
                    has_aggregated_neighbors = true;
                    break;
                }
            }
        }    

        if(!has_neighbors){
            //isolated node, do not aggregate
            x[i] = -n_row;
        }
        else if (!has_aggregated_neighbors){
            //Make an aggregate out of this node and its neighbors
            x[i] = next_aggregate;
            y[next_aggregate-1] = i;              //y stores a list of the Cpts
            for(I jj = row_start; jj < row_end; jj++){
                x[Aj[jj]] = next_aggregate;
            }
            next_aggregate++;
        }
    }

    //Pass #2
    // Add unaggregated nodes to any neighboring aggregate
    for(I i = 0; i < n_row; i++){
        if(x[i]){ continue; } //already marked

        for(I jj = Ap[i]; jj < Ap[i+1]; jj++){
            const I j = Aj[jj];
        
            const I xj = x[j];
            if(xj > 0){
                x[i] = -xj;
                break;
            }
        }
    }
   
    next_aggregate--; 
    
    //Pass #3
    for(I i = 0; i < n_row; i++){
        const I xi = x[i];

        if(xi != 0){ 
            // node i has been aggregated
            if(xi > 0)
                x[i] = xi - 1;
            else if(xi == -n_row)
                x[i] = -1;
            else
                x[i] = -xi - 1;
            continue;
        }

        // node i has not been aggregated
        const I row_start = Ap[i];
        const I row_end   = Ap[i+1];

        x[i] = next_aggregate;
        y[next_aggregate] = i;              //y stores a list of the Cpts

        for(I jj = row_start; jj < row_end; jj++){
            const I j = Aj[jj];

            if(x[j] == 0){ //unmarked neighbors
                x[j] = next_aggregate;
            }
        }  
        next_aggregate++;
    }
    

    return next_aggregate; //number of aggregates
}



/*
 * Compute aggregates for a matrix A stored in CSR format
 *
 * Parameters:
 *   n_row         - number of rows in A
 *   Ap[n_row + 1] - CSR row pointer
 *   Aj[nnz]       - CSR column indices
 *    x[n_row]     - aggregate numbers for each node
 *    y[n_row]     - will hold Cpts upon return
 *
 * Returns:
 *  The number of aggregates (== max(x[:]) + 1 )
 *
 * Notes:
 * Differs from standard aggregation.  Each dof is considered.  
 * If it has been aggregated, skip over.  Otherwise, put dof 
 * and any unaggregated neighbors in an aggregate.  Results 
 * in possibly much higher complexities.
 */
template <class I>
I naive_aggregation(const I n_row,
                       const I Ap[], 
                       const I Aj[],
                             I  x[],
                             I  y[])
{
    // x[n] == 0 means i-th node has not been aggregated
    std::fill(x, x + n_row, 0);

    I next_aggregate = 1; // number of aggregates + 1

    for(I i = 0; i < n_row; i++){
        if(x[i]){ continue; } //already marked
        else
        {
            const I row_start = Ap[i];
            const I row_end   = Ap[i+1];

           //Make an aggregate out of this node and its unaggregated neighbors
           x[i] = next_aggregate;
           for(I jj = row_start; jj < row_end; jj++){
               if(!x[Aj[jj]]){
                   x[Aj[jj]] = next_aggregate;}
           }
           
           //y stores a list of the Cpts
           y[next_aggregate-1] = i;              
           next_aggregate++;
        }
    }

    return (next_aggregate-1); //number of aggregates
}


/*
 *  Given a set of near-nullspace candidates stored in the columns of B, and
 *  an aggregation operator stored in A using BSR format, this method computes
 *      Ax : the data array of the tentative prolongator in BSR format
 *      R : the coarse level near-nullspace candidates
 *
 *  The tentative prolongator A and coarse near-nullspaces candidates satisfy 
 *  the following relationships:
 *      B = A * R        and      transpose(A) * A = identity
 *
 *  Parameters
 *      num_rows   - number of rows in A
 *      num_cols   - number of columns in A
 *      K1         - BSR row blocksize
 *      K2         - BSR column blocksize
 *      Ap[]       - BSR row pointer
 *      Aj[]       - BSR index array
 *      Ax[]       - BSR data array
 *      B[]        - fine-level near-nullspace candidates (n_row x K2)
 *      R[]        - coarse-level near-nullspace candidates (n_coarse x K2)
 *      tol        - tolerance used to drop numerically linearly dependent vectors
 *
 *  
 *  Returns:
 *      Nothing, Ax and R will be modified in places.
 *
 *  Notes:
 *      - Storage for Ax and R must be preallocated.
 *      - The tol parameter is applied to the candidates restricted to each
 *      aggregate to discard (redundant) numerically linear dependencies. 
 *      For instance, if the restriction of two different fine-level candidates
 *      to a single aggregate are equal, then the second candidate will not
 *      contribute to the range of A.
 *      - When the aggregation operator does not aggregate all fine-level
 *      nodes, the corresponding rows of A will simply be zero.  In this case,
 *      the two relationships mentioned above do not hold.  Instead the following
 *      relationships are maintained:
 *             B[i,:] = A[i,:] * R     where  A[i,:] is nonzero
 *         and
 *             transpose(A[i,:]) * A[i,:] = 1   where A[i,:] is nonzero
 *
 */
template <class I, class S, class T>
void fit_candidates_common(const I n_row,
                           const I n_col,
                           const I   K1,
                           const I   K2,
                           const I Ap[], 
                           const I Ai[],
                                 T Ax[],
                           const T  B[],
                                 T  R[],
                           const S  tol)
{
    std::fill(R, R + (n_col*K2*K2), 0);


    const I BS = K1*K2; //blocksize

    //Copy blocks into Ax
    for(I j = 0; j < n_col; j++){
        T * Ax_start = Ax + BS * Ap[j];

        for(I ii = Ap[j]; ii < Ap[j+1]; ii++){
            const T * B_start = B + BS*Ai[ii];
            const T * B_end   = B_start + BS;
            std::copy(B_start, B_end, Ax_start);
            Ax_start += BS;
        }
    }

    
    //orthonormalize columns
    for(I j = 0; j < n_col; j++){
        const I col_start  = Ap[j];
        const I col_end    = Ap[j+1];

        T * Ax_start = Ax + BS * col_start;
        T * Ax_end   = Ax + BS * col_end;
        T * R_start  = R  + j * K2 * K2;
        
        for(I bj = 0; bj < K2; bj++){
            //compute norm of block column
            S norm_j = 0;

            {
                T * Ax_col = Ax_start + bj;
                while(Ax_col < Ax_end){
                    norm_j += mynorm(*Ax_col);
                    Ax_col += K2;
                }
                norm_j = std::sqrt(norm_j);
            }
            
            const S threshold_j = tol * norm_j;
    
            //orthogonalize bj against previous columns
            for(I bi = 0; bi < bj; bi++){

                //compute dot product with column bi
                T dot_prod = 0;

                {
                    T * Ax_bi = Ax_start + bi;
                    T * Ax_bj = Ax_start + bj;
                    while(Ax_bi < Ax_end){
                        dot_prod += dot(*Ax_bj,*Ax_bi);
                        Ax_bi    += K2;
                        Ax_bj    += K2;
                    }
                }

                // orthogonalize against column bi
                {
                    T * Ax_bi = Ax_start + bi;
                    T * Ax_bj = Ax_start + bj;
                    while(Ax_bi < Ax_end){
                        *Ax_bj -= dot_prod * (*Ax_bi);
                        Ax_bi  += K2;
                        Ax_bj  += K2;
                    }
                }
                
                R_start[K2 * bi + bj] = dot_prod;
            } // end orthogonalize bj against previous columns
            

            //compute norm of column bj
            norm_j = 0;
            {
                T * Ax_bj = Ax_start + bj;
                while(Ax_bj < Ax_end){
                    norm_j += mynorm(*Ax_bj);
                    Ax_bj  += K2;
                }
                norm_j = std::sqrt(norm_j);
            }
           

            //normalize column bj if, after orthogonalization, its
            //euclidean norm exceeds the threshold. Otherwise set 
            //column bj to 0.
            T scale;
            if(norm_j > threshold_j){
                scale = 1.0/norm_j;
                R_start[K2 * bj + bj] = norm_j;
            } else {
                scale = 0;
                
                // JBS code...explicitly zero out this column of R
                //for(I bi = 0; bi <= bj; bi++){
                //    R_start[K2 * bi + bj] = 0.0; 
                //}
                // Nathan's code that just sets the diagonal entry of R to 0
                R_start[K2 * bj + bj] = 0;
            }
            {
                T * Ax_bj = Ax_start + bj;
                while(Ax_bj < Ax_end){
                    *Ax_bj *= scale;
                    Ax_bj  += K2;
                }
            }

        } // end orthogonalizing block column j
    }
}


/*
 * Helper routine for satisfy_constraints routine called 
 *     by energy_prolongation_smoother(...) in smooth.py
 * This implements the python code:
 *
 *   # U is a BSR matrix, B is num_block_rows x ColsPerBlock x ColsPerBlock
 *   # UB is num_block_rows x RowsPerBlock x ColsPerBlock,  BtBinv is 
 *        num_block_rows x ColsPerBlock x ColsPerBlock
 *   B  = asarray(B).reshape(-1,ColsPerBlock,B.shape[1])
 *   UB = asarray(UB).reshape(-1,RowsPerBlock,UB.shape[1])
 *
 *   rows = csr_matrix((U.indices,U.indices,U.indptr), \
 *           shape=(U.shape[0]/RowsPerBlock,U.shape[1]/ColsPerBlock)).tocoo(copy=False).row
 *   for n,j in enumerate(U.indices):
 *      i = rows[n]
 *      Bi  = mat(B[j])
 *      UBi = UB[i]
 *      U.data[n] -= dot(UBi,dot(BtBinv[i],Bi.H))
 *
 * Parameters
 * ----------
 * RowsPerBlock : {int}
 *      rows per block in the BSR matrix, S
 * ColsPerBlock : {int}
 *      cols per block in the BSR matrix, S
 * num_block_rows : {int}
 *      Number of block rows, S.shape[0]/RowsPerBlock
 * NullDim : {int}
 *      Null-space dimension, i.e., the number of columns in B
 * x : {float|complex array}
 *      Conjugate of near-nullspace vectors, B, in row major
 * y : {float|complex array}
 *      S*B, in row major
 * z : {float|complex array}
 *      BtBinv, in row major, i.e. z[i] = pinv(B_i.H Bi), where
 *      B_i is B restricted to the neighborhood of dof of i.
 * Sp : {int array}
 *      Row pointer array for BSR matrix S
 * Sj : {int array}
 *      Col index array for BSR matrix S
 * Sx : {float|complex array}
 *      Value array for BSR matrix S
 *  
 * Return
 * ------
 * Sx is modified such that S*B = 0.  S ends up being the 
 * update to the prolongator in the energy_minimization algorithm.
 *
 * Notes
 * -----
 * Principle calling routine is energy_prolongation_smoother(...) in smooth.py.  
 *
 */          

template<class I, class T, class F>
void satisfy_constraints_helper(const I RowsPerBlock,  const I ColsPerBlock, 
                                 const I num_block_rows,const I NullDim,      
                                 const T x[],           const T y[], 
                                 const T z[],           const I Sp[],         
                                 const I Sj[],                T Sx[])
{
    //Rename to something more familiar
    const T * Bt = x;
    const T * UB = y;
    const T * BtBinv = z;
    
    //Declare
    I BlockSize = RowsPerBlock*ColsPerBlock;
    I NullDimSq = NullDim*NullDim;
    I NullDim_Cols = NullDim*ColsPerBlock;
    I NullDim_Rows = NullDim*RowsPerBlock;

    //C will store an intermediate mat-mat product
    std::vector<T> Update(BlockSize,0);
    std::vector<T> C(NullDim_Cols,0);
    for(I i = 0; i < NullDim_Cols; i++)
    {   C[i] = 0.0; }

    //Begin Main Loop
    for(I i = 0; i < num_block_rows; i++)
    {
        I rowstart = Sp[i]; 
        I rowend = Sp[i+1];

        for(I j = rowstart; j < rowend; j++)
        {
            // Calculate C = BtBinv[i*NullDimSq => (i+1)*NullDimSq]  *  B[ Sj[j]*blocksize => (Sj[j]+1)*blocksize ]^H
            // Implicit transpose of conjugate(B_i) is done through gemm assuming Bt is in column major
            gemm(&(BtBinv[i*NullDimSq]), NullDim, NullDim, 'F', &(Bt[Sj[j]*NullDim_Cols]), NullDim, ColsPerBlock, 'F', &(C[0]), NullDim, ColsPerBlock, 'T', 'T');

            // Calculate Sx[ j*BlockSize => (j+1)*blocksize ] =  UB[ i*BlockSize => (i+1)*blocksize ] * C
            // Note that C actually stores C^T in row major, or C in col major.  gemm assumes C is in col major, so we're OK
            gemm(&(UB[i*NullDim_Rows]), RowsPerBlock, NullDim, 'F', &(C[0]), NullDim, ColsPerBlock, 'F', &(Update[0]), RowsPerBlock, ColsPerBlock, 'F', 'T');
            
            //Update Sx
            for(I k = 0; k < BlockSize; k++)
            {   Sx[j*BlockSize + k] -= Update[k]; }
        }
    }
}


/*
 * Helper routine for energy_prolongation_smoother
 * Calculates the following python code:
 *
 *   RowsPerBlock = Sparsity_Pattern.blocksize[0]
 *   BtB = zeros((Nnodes,NullDim,NullDim), dtype=B.dtype)
 *   S2 = Sparsity_Pattern.tocsr()
 *   for i in range(Nnodes):
 *       Bi = mat( B[S2.indices[S2.indptr[i*RowsPerBlock]:S2.indptr[i*RowsPerBlock + 1]],:] )
 *       BtB[i,:,:] = Bi.H*Bi 
 *
 * Parameters
 * ----------
 * NullDim : {int}
 *      Number of near nullspace vectors
 * Nnodes : {int}
 *      Number of nodes, i.e. number of block rows in BSR matrix, S
 * ColsPerBlock : {int}
 *      Columns per block in S
 * b : {float|complex array}
 *      Nnodes x BsqCols array, in row-major form.
 *      This is B-squared, i.e. it is each column of B 
 *      multiplied against each other column of B.  For a Nx3 B,
 *      b[:,0] = conjugate(B[:,0])*B[:,0]
 *      b[:,1] = conjugate(B[:,0])*B[:,1]
 *      b[:,2] = conjugate(B[:,0])*B[:,2]
 *      b[:,3] = conjugate(B[:,1])*B[:,1]
 *      b[:,4] = conjugate(B[:,1])*B[:,2]
 *      b[:,5] = conjugate(B[:,2])*B[:,2]
 * BsqCols : {int}
 *      sum(range(NullDim+1)), i.e. number of columns in b
 * x  : {float|complex array}
 *      Modified inplace for output.  Should be zeros upon entry
 * Sp,Sj : {int array} 
 *      BSR indptr and indices members for matrix, S
 *
 * Return
 * ------
 * BtB[i] = B_i.H*B_i in __column__ major format
 * where B_i is B[colindices,:], colindices = all the nonzero
 * column indices for block row i in S
 *
 * Notes
 * -----
 * Principle calling routine is energy_prolongation_smoother(...) in smooth.py.  
 *
 */          
template<class I, class T, class F>
void calc_BtB(const I NullDim, const I Nnodes,  const I ColsPerBlock, 
                const T b[],     const I BsqCols, T x[], 
                const I Sp[],    const I Sj[])
{
    //Rename to something more familiar
    const T * Bsq = b;
    T * BtB = x;
    
    //Declare workspace
    //const I NullDimLoc = NullDim;
    const I NullDimSq  = NullDim*NullDim;
    const I work_size  = 5*NullDim + 10;

    T * BtB_loc   = new T[NullDimSq];
    T * work      = new T[work_size];
    
    //Loop over each row
    for(I i = 0; i < Nnodes; i++)
    {
        const I rowstart = Sp[i];
        const I rowend   = Sp[i+1];
        for(I k = 0; k < NullDimSq; k++)
        {   BtB_loc[k] = 0.0; }
        
        //Loop over row i in order to calculate B_i^H*B_i, where B_i is B 
        // with the rows restricted only to the nonzero column indices of row i of S
        for(I j = rowstart; j < rowend; j++)
        {
            // Calculate absolute column index start and stop 
            //  for block column j of BSR matrix, S
            const I colstart = Sj[j]*ColsPerBlock;
            const I colend   = colstart + ColsPerBlock;

            //Loop over each absolute column index, k, of block column, j
            for(I k = colstart; k < colend; k++)
            {          
                // Do work in computing Diagonal of  BtB_loc  
                I BtBcounter = 0; 
                I BsqCounter = k*BsqCols;                   // Row-major index
                for(I m = 0; m < NullDim; m++)
                {
                    BtB_loc[BtBcounter] += Bsq[BsqCounter];
                    BtBcounter += NullDim + 1;
                    BsqCounter += (NullDim - m);
                }
                // Do work in computing off-diagonals of BtB_loc, noting that BtB_loc is Hermitian and that
                // svd_solve needs BtB_loc in column-major form, because svd_solve is Fortran
                BsqCounter = k*BsqCols;
                for(I m = 0; m < NullDim; m++)  // Loop over cols
                {
                    I counter = 1;
                    for(I n = m+1; n < NullDim; n++) // Loop over Rows
                    {
                        T elmt_bsq = Bsq[BsqCounter + counter];
                        BtB_loc[m*NullDim + n] += conjugate(elmt_bsq);      // entry(n, m)
                        BtB_loc[n*NullDim + m] += elmt_bsq;                 // entry(m, n)
                        counter ++;
                    }
                    BsqCounter += (NullDim - m);
                }
            } // end k loop
        } // end j loop

        // Copy BtB_loc into BtB at the ptr location offset by i*NullDimSq
        // Note that we are moving the data from column major in BtB_loc to column major in curr_block.
        T * curr_block = BtB + i*NullDimSq; 
        for(I k = 0; k < NullDimSq; k++)
        {   curr_block[k] = BtB_loc[k]; }
    
    } // end i loop

    delete[] BtB_loc; 
    delete[] work;
}

/*
 * Calculate A*B = S, but only at the pre-existing sparsity
 * pattern of S, i.e. do an exact, but incomplete mat-mat mult.
 *
 * A, B and S must all be in BSR, may be rectangular, but the 
 * indices need not be sorted. 
 * Also, A.blocksize[0] must equal S.blocksize[0]
 *       A.blocksize[1] must equal B.blocksize[0]
 *       B.blocksize[1] must equal S.blocksize[1]
 *
 * Parameters
 * ----------
 * Ap : {int array}
 *      BSR row pointer array
 * Aj : {int array}
 *      BSR col index array
 * Ax : {float|complex array}
 *      BSR value array
 * Bp : {int array}
 *      BSR row pointer array                 
 * Bj : {int array}
 *      BSR col index array                 
 * Bx : {float|complex array}
 *      BSR value array
 * Sp : {int array}
 *      BSR row pointer array
 * Sj : {int array}
 *      BSR col index array
 * Sx : {float|complex array}
 *      BSR value array     
 * n_brow : {int}
 *      Number of block-rows in A
 * n_bcol : {int}
 *      Number of block-cols in S
 * brow_A : {int}
 *      row blocksize for A
 * bcol_A : {int}
 *      column blocksize for A
 * bcol_B : {int}
 *      column blocksize for B
 *
 * Returns
 * -------
 * Sx is modified in-place to reflect S(i,j) = <A_{i,:}, B_{:,j}>
 * but only for those entries already present in the sparsity pattern
 * of S.
 *
 * Notes
 * -----
 * 
 * Algorithm is SMMP
 *
 * Principle calling routine is energy_prolongation_smoother(...) in
 * smooth.py.  Here it is used to calculate the descent direction
 * A*P_tent, but only within an accepted sparsity pattern.
 *
 * Is generally faster than the commented out incomplete_BSRmatmat(...)
 * routine below, except when S has far few nonzeros than A or B.
 *
 */
template<class I, class T, class F>
void incomplete_mat_mult_bsr( const I Ap[],   const I Aj[],    const T Ax[], 
                            const I Bp[],   const I Bj[],    const T Bx[], 
                            const I Sp[],   const I Sj[],          T Sx[], 
                            const I n_brow, const I n_bcol,  const I brow_A,  
                            const I bcol_A, const I bcol_B )
{
    
    std::vector<T*> S(n_bcol);
    std::fill(S.begin(), S.end(), (T *) NULL);
    
    I A_blocksize = brow_A*bcol_A;
    I B_blocksize = bcol_A*bcol_B;
    I S_blocksize = brow_A*bcol_B;
    I one_by_one_blocksize = 0;
    if ((A_blocksize == B_blocksize) && (B_blocksize == S_blocksize) && (A_blocksize == 1)){
        one_by_one_blocksize = 1; }

    // Loop over rows of A
    for(I i = 0; i < n_brow; i++){

        // Initialize S to be NULL, except for the nonzero entries in S[i,:], 
        // where S will point to the correct location in Sx
        I jj_start = Sp[i];
        I jj_end   = Sp[i+1];
        for(I jj = jj_start; jj < jj_end; jj++){
            S[ Sj[jj] ] = &(Sx[jj*S_blocksize]); }

        // Loop over columns in row i of A
        jj_start = Ap[i];
        jj_end   = Ap[i+1];
        for(I jj = jj_start; jj < jj_end; jj++){
            I j = Aj[jj];
            
            // Loop over columns in row j of B
            I kk_start = Bp[j];
            I kk_end   = Bp[j+1];
            for(I kk = kk_start; kk < kk_end; kk++){
                I k = Bj[kk];
                T * Sk = S[k];
                
                // If this is an allowed entry in S, then accumulate to it with a block multiply
                if (Sk != NULL){
                    if(one_by_one_blocksize){
                        // Just do a scalar multiply for the case of 1x1 blocks
                        *(Sk) += Ax[jj]*Bx[kk];
                    }
                    else{ 
                        gemm(&(Ax[jj*A_blocksize]), brow_A, bcol_A, 'F', 
                             &(Bx[kk*B_blocksize]), bcol_A, bcol_B, 'T', 
                             Sk,                    brow_A, bcol_B, 'F',
                             'F'); 
                    }
                }
            }
        }  
        
        // Revert S back to it's state of all NULL
        jj_start = Sp[i];
        jj_end   = Sp[i+1];
        for(I jj = jj_start; jj < jj_end; jj++){
            S[ Sj[jj] ] = NULL; }

    }
}


#endif