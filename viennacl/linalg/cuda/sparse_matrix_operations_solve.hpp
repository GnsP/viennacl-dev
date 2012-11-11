#ifndef VIENNACL_LINALG_CUDA_SPARSE_MATRIX_OPERATIONS_SOLVE_HPP_
#define VIENNACL_LINALG_CUDA_SPARSE_MATRIX_OPERATIONS_SOLVE_HPP_

/* =========================================================================
   Copyright (c) 2010-2012, Institute for Microelectronics,
                            Institute for Analysis and Scientific Computing,
                            TU Wien.

                            -----------------
                  ViennaCL - The Vienna Computing Library
                            -----------------

   Project Head:    Karl Rupp                   rupp@iue.tuwien.ac.at
               
   (A list of authors and contributors can be found in the PDF manual)

   License:         MIT (X11), see file LICENSE in the base directory
============================================================================= */

/** @file viennacl/linalg/cuda/sparse_matrix_operations_solve.hpp
    @brief Implementations of direct triangular solvers for sparse matrices using CUDA
*/

#include "viennacl/forwards.h"

namespace viennacl
{
  namespace linalg
  {
    namespace cuda
    {
      //
      // Compressed matrix
      //

      //
      // non-transposed
      //
      
      template <typename T>
      __global__ void csr_unit_lu_forward_kernel(
                const unsigned int * row_indices,
                const unsigned int * column_indices, 
                const T * elements,
                      T * vector,
                unsigned int size) 
      {
        __shared__  unsigned int col_index_buffer[128];
        __shared__  T element_buffer[128];
        __shared__  T vector_buffer[128];
        
        unsigned int nnz = row_indices[size];
        unsigned int current_row = 0;
        unsigned int row_at_window_start = 0;
        T current_vector_entry = vector[0];
        unsigned int loop_end = (nnz / blockDim.x + 1) * blockDim.x;
        unsigned int next_row = row_indices[1];
        
        for (unsigned int i = threadIdx.x; i < loop_end; i += blockDim.x)
        {
          //load into shared memory (coalesced access):
          if (i < nnz)
          {
            element_buffer[threadIdx.x] = elements[i];
            unsigned int tmp = column_indices[i];
            col_index_buffer[threadIdx.x] = tmp;
            vector_buffer[threadIdx.x] = vector[tmp];
          }
          
          __syncthreads();
          
          //now a single thread does the remaining work in shared memory:
          if (threadIdx.x == 0)
          {
            // traverse through all the loaded data:
            for (unsigned int k=0; k<blockDim.x; ++k)
            {
              if (current_row < size && i+k == next_row) //current row is finished. Write back result
              {
                vector[current_row] = current_vector_entry;
                ++current_row;
                if (current_row < size) //load next row's data
                {
                  next_row = row_indices[current_row+1];
                  current_vector_entry = vector[current_row];
                }
              }
              
              if (current_row < size && col_index_buffer[k] < current_row) //substitute
              {
                if (col_index_buffer[k] < row_at_window_start) //use recently computed results
                  current_vector_entry -= element_buffer[k] * vector_buffer[k];
                else if (col_index_buffer[k] < current_row) //use buffered data
                  current_vector_entry -= element_buffer[k] * vector[col_index_buffer[k]];
              }

            } // for k
            
            row_at_window_start = current_row;
          } // if (get_local_id(0) == 0)
          
          __syncthreads();
        } //for i
      }



      template <typename T>
      __global__ void csr_lu_forward_kernel(
                const unsigned int * row_indices,
                const unsigned int * column_indices, 
                const T * elements,
                const T * diagonal_entries,
                      T * vector,
                unsigned int size) 
      {
        __shared__  unsigned int col_index_buffer[128];
        __shared__  T element_buffer[128];
        __shared__  T vector_buffer[128];
        
        unsigned int nnz = row_indices[size];
        unsigned int current_row = 0;
        unsigned int row_at_window_start = 0;
        T current_vector_entry = vector[0];
        unsigned int loop_end = (nnz / blockDim.x + 1) * blockDim.x;
        unsigned int next_row = row_indices[1];
        
        for (unsigned int i = threadIdx.x; i < loop_end; i += blockDim.x)
        {
          //load into shared memory (coalesced access):
          if (i < nnz)
          {
            element_buffer[threadIdx.x] = elements[i];
            unsigned int tmp = column_indices[i];
            col_index_buffer[threadIdx.x] = tmp;
            vector_buffer[threadIdx.x] = vector[tmp];
          }
          
          __syncthreads();
          
          //now a single thread does the remaining work in shared memory:
          if (threadIdx.x == 0)
          {
            // traverse through all the loaded data:
            for (unsigned int k=0; k<blockDim.x; ++k)
            {
              if (current_row < size && i+k == next_row) //current row is finished. Write back result
              {
                vector[current_row] = current_vector_entry / diagonal_entries[current_row];
                ++current_row;
                if (current_row < size) //load next row's data
                {
                  next_row = row_indices[current_row+1];
                  current_vector_entry = vector[current_row];
                }
              }
              
              if (current_row < size && col_index_buffer[k] < current_row) //substitute
              {
                if (col_index_buffer[k] < row_at_window_start) //use recently computed results
                  current_vector_entry -= element_buffer[k] * vector_buffer[k];
                else if (col_index_buffer[k] < current_row) //use buffered data
                  current_vector_entry -= element_buffer[k] * vector[col_index_buffer[k]];
              }

            } // for k
            
            row_at_window_start = current_row;
          } // if (get_local_id(0) == 0)
          
          __syncthreads();
        } //for i
      }

      
      template <typename T>
      __global__ void csr_unit_lu_backward_kernel(
                const unsigned int * row_indices,
                const unsigned int * column_indices, 
                const T * elements,
                      T * vector,
                unsigned int size) 
      {
        __shared__  unsigned int col_index_buffer[128];
        __shared__  T element_buffer[128];
        __shared__  T vector_buffer[128];
        
        unsigned int nnz = row_indices[size];
        unsigned int current_row = size-1;
        unsigned int row_at_window_start = size-1;
        T current_vector_entry = vector[size-1];
        unsigned int loop_end = ( (nnz - 1) / blockDim.x) * blockDim.x;
        unsigned int next_row = row_indices[size-1];
        
        unsigned int i = loop_end + threadIdx.x;
        while (1)
        {
          //load into shared memory (coalesced access):
          if (i < nnz)
          {
            element_buffer[threadIdx.x] = elements[i];
            unsigned int tmp = column_indices[i];
            col_index_buffer[threadIdx.x] = tmp;
            vector_buffer[threadIdx.x] = vector[tmp];
          }
          
          __syncthreads();
          
          //now a single thread does the remaining work in shared memory:
          if (threadIdx.x == 0)
          {
            // traverse through all the loaded data from back to front:
            for (unsigned int k2=0; k2<blockDim.x; ++k2)
            {
              unsigned int k = (blockDim.x - k2) - 1;
              
              if (i+k >= nnz)
                continue;
              
              if (col_index_buffer[k] > row_at_window_start) //use recently computed results
                current_vector_entry -= element_buffer[k] * vector_buffer[k];
              else if (col_index_buffer[k] > current_row) //use buffered data
                current_vector_entry -= element_buffer[k] * vector[col_index_buffer[k]];
              
              if (i+k == next_row) //current row is finished. Write back result
              {
                vector[current_row] = current_vector_entry;
                if (current_row > 0) //load next row's data
                {
                  --current_row;
                  next_row = row_indices[current_row];
                  current_vector_entry = vector[current_row];
                }
              }
              
              
            } // for k
            
            row_at_window_start = current_row;
          } // if (get_local_id(0) == 0)
          
          __syncthreads();
          
          if (i < blockDim.x)
            break;
          
          i -= blockDim.x;
        } //for i
      }

      

      template <typename T>
      __global__ void csr_lu_backward_kernel(
                const unsigned int * row_indices,
                const unsigned int * column_indices, 
                const T * elements,
                      T * vector,
                unsigned int size) 
      {
        __shared__  unsigned int col_index_buffer[128];
        __shared__  T element_buffer[128];
        __shared__  T vector_buffer[128];
        
        unsigned int nnz = row_indices[size];
        unsigned int current_row = size-1;
        unsigned int row_at_window_start = size-1;
        T current_vector_entry = vector[size-1];
        T diagonal_entry;
        unsigned int loop_end = ( (nnz - 1) / blockDim.x) * blockDim.x;
        unsigned int next_row = row_indices[size-1];
        
        unsigned int i = loop_end + threadIdx.x;
        while (1)
        {
          //load into shared memory (coalesced access):
          if (i < nnz)
          {
            element_buffer[threadIdx.x] = elements[i];
            unsigned int tmp = column_indices[i];
            col_index_buffer[threadIdx.x] = tmp;
            vector_buffer[threadIdx.x] = vector[tmp];
          }
          
          __syncthreads();
          
          //now a single thread does the remaining work in shared memory:
          if (threadIdx.x == 0)
          {
            // traverse through all the loaded data from back to front:
            for (unsigned int k2=0; k2<blockDim.x; ++k2)
            {
              unsigned int k = (blockDim.x - k2) - 1;
              
              if (i+k >= nnz)
                continue;
              
              if (col_index_buffer[k] > row_at_window_start) //use recently computed results
                current_vector_entry -= element_buffer[k] * vector_buffer[k];
              else if (col_index_buffer[k] > current_row) //use buffered data
                current_vector_entry -= element_buffer[k] * vector[col_index_buffer[k]];
              else if (col_index_buffer[k] == current_row)
                diagonal_entry = element_buffer[k];
              
              if (i+k == next_row) //current row is finished. Write back result
              {
                vector[current_row] = current_vector_entry / diagonal_entry;
                if (current_row > 0) //load next row's data
                {
                  --current_row;
                  next_row = row_indices[current_row];
                  current_vector_entry = vector[current_row];
                }
              }
              
              
            } // for k
            
            row_at_window_start = current_row;
          } // if (get_local_id(0) == 0)
          
          __syncthreads();
          
          if (i < blockDim.x)
            break;
          
          i -= blockDim.x;
        } //for i
      }

      
      
      //
      // transposed
      //
      
      
      template <typename T>
      __global__ void csr_trans_lu_forward_kernel2(
                const unsigned int * row_indices,
                const unsigned int * column_indices, 
                const T * elements,
                      T * vector,
                unsigned int size) 
      {
        for (unsigned int row = 0; row < size; ++row) 
        { 
          T result_entry = vector[row]; 
          
          unsigned int row_start = row_indices[row]; 
          unsigned int row_stop  = row_indices[row + 1];
          for (unsigned int entry_index = row_start + threadIdx.x; entry_index < row_stop; entry_index += blockDim.x) 
          {
            unsigned int col_index = column_indices[entry_index];
            if (col_index > row)
              vector[col_index] -= result_entry * elements[entry_index]; 
          }
          
          __syncthreads();
        } 
      }      
      
      template <typename T>
      __global__ void csr_trans_unit_lu_forward_kernel(
                const unsigned int * row_indices,
                const unsigned int * column_indices, 
                const T * elements,
                      T * vector,
                unsigned int size) 
      {
        __shared__  unsigned int row_index_lookahead[256];
        __shared__  unsigned int row_index_buffer[256];
        
        unsigned int row_index;
        unsigned int col_index;
        T matrix_entry;
        unsigned int nnz = row_indices[size];
        unsigned int row_at_window_start = 0;
        unsigned int row_at_window_end = 0;
        unsigned int loop_end = ( (nnz - 1) / blockDim.x + 1) * blockDim.x;
        
        for (unsigned int i = threadIdx.x; i < loop_end; i += blockDim.x)
        {
          col_index    = (i < nnz) ? column_indices[i] : 0;
          matrix_entry = (i < nnz) ? elements[i]       : 0;
          row_index_lookahead[threadIdx.x] = (row_at_window_start + threadIdx.x < size) ? row_indices[row_at_window_start + threadIdx.x] : size - 1;

          __syncthreads();
          
          if (i < nnz)
          {
            unsigned int row_index_inc = 0;
            while (i >= row_index_lookahead[row_index_inc + 1])
              ++row_index_inc;
            row_index = row_at_window_start + row_index_inc;
            row_index_buffer[threadIdx.x] = row_index;
          }
          else
          {
            row_index = size+1;
            row_index_buffer[threadIdx.x] = size - 1;
          }
          
          __syncthreads();
          
          row_at_window_start = row_index_buffer[0];
          row_at_window_end   = row_index_buffer[blockDim.x - 1];
          
          //forward elimination
          for (unsigned int row = row_at_window_start; row <= row_at_window_end; ++row) 
          { 
            T result_entry = vector[row];
            
            if ( (row_index == row) && (col_index > row) )
              vector[col_index] -= result_entry * matrix_entry; 

            __syncthreads();
          }
          
          row_at_window_start = row_at_window_end;
        }
          
      }

      template <typename T>
      __global__ void csr_trans_lu_forward_kernel(
                const unsigned int * row_indices,
                const unsigned int * column_indices, 
                const T * elements,
                const T * diagonal_entries,
                      T * vector,
                unsigned int size) 
      {
        __shared__  unsigned int row_index_lookahead[256];
        __shared__  unsigned int row_index_buffer[256];
        
        unsigned int row_index;
        unsigned int col_index;
        T matrix_entry;
        unsigned int nnz = row_indices[size];
        unsigned int row_at_window_start = 0;
        unsigned int row_at_window_end = 0;
        unsigned int loop_end = ( (nnz - 1) / blockDim.x + 1) * blockDim.x;
        
        for (unsigned int i = threadIdx.x; i < loop_end; i += blockDim.x)
        {
          col_index    = (i < nnz) ? column_indices[i] : 0;
          matrix_entry = (i < nnz) ? elements[i]       : 0;
          row_index_lookahead[threadIdx.x] = (row_at_window_start + threadIdx.x < size) ? row_indices[row_at_window_start + threadIdx.x] : size - 1;

          __syncthreads();
          
          if (i < nnz)
          {
            unsigned int row_index_inc = 0;
            while (i >= row_index_lookahead[row_index_inc + 1])
              ++row_index_inc;
            row_index = row_at_window_start + row_index_inc;
            row_index_buffer[threadIdx.x] = row_index;
          }
          else
          {
            row_index = size+1;
            row_index_buffer[threadIdx.x] = size - 1;
          }
          
          __syncthreads();
          
          row_at_window_start = row_index_buffer[0];
          row_at_window_end   = row_index_buffer[blockDim.x - 1];
          
          //forward elimination
          for (unsigned int row = row_at_window_start; row <= row_at_window_end; ++row) 
          { 
            T result_entry = vector[row] / diagonal_entries[row];
            vector[row] = result_entry;
            
            if ( (row_index == row) && (col_index > row) )
              vector[col_index] -= result_entry * matrix_entry; 

            __syncthreads();
          }
          
          row_at_window_start = row_at_window_end;
        }
          
      }
      
      
      template <typename T>
      __global__ void csr_trans_unit_lu_backward_kernel(
                const unsigned int * row_indices,
                const unsigned int * column_indices, 
                const T * elements,
                      T * vector,
                unsigned int size) 
      {
        __shared__  unsigned int row_index_lookahead[256];
        __shared__  unsigned int row_index_buffer[256];
        
        unsigned int row_index;
        unsigned int col_index;
        T matrix_entry;
        unsigned int nnz = row_indices[size];
        unsigned int row_at_window_start = 0;
        unsigned int row_at_window_end = 0;
        unsigned int loop_end = ( (nnz - 1) / blockDim.x + 1) * blockDim.x;
        
        for (unsigned int i2 = threadIdx.x; i2 < loop_end; i2 += blockDim.x)
        {
          unsigned i = (loop_end - i2) - 1;
          col_index    = (i < nnz) ? column_indices[i] : 0;
          matrix_entry = (i < nnz) ? elements[i]       : 0;
          row_index_lookahead[threadIdx.x] = (row_at_window_start >= blockDim - threadIdx.x) ? row_indices[row_at_window_start - (blockDim - threadIdx.x - 1)] : 0;

          __syncthreads();
          
          if (i < nnz)
          {
            unsigned int row_index_dec = 0;
            while (row_index_lookahead[blockDim.x - row_index_dec - 1] > i)
              ++row_index_dec;
            row_index = row_at_window_start - row_index_dec;
            row_index_buffer[threadIdx.x] = row_index;
          }
          else
          {
            row_index = size+1;
            row_index_buffer[threadIdx.x] = size - 1;
          }
          
          __syncthreads();
          
          row_at_window_start = row_index_buffer[0];
          row_at_window_end   = row_index_buffer[blockDim.x - 1];
          
          //backward elimination
          for (unsigned int row = row_at_window_start; row <= row_at_window_end; ++row) 
          { 
            T result_entry = vector[row];
            
            if ( (row_index == row) && (col_index < row) )
              vector[col_index] -= result_entry * matrix_entry; 

            __syncthreads();
          }
          
          row_at_window_start = row_at_window_end;
        }
          
      }
      
      
      
      template <typename T>
      __global__ void csr_trans_lu_backward_kernel2(
                const unsigned int * row_indices,
                const unsigned int * column_indices, 
                const T * elements,
                const T * diagonal_entries,
                      T * vector,
                unsigned int size) 
      {
        T result_entry = 0;
        
        //backward elimination, using U and D: 
        for (unsigned int row2 = 0; row2 < size; ++row2) 
        { 
          unsigned int row = (size - row2) - 1;
          result_entry = vector[row] / diagonal_entries[row]; 
          
          unsigned int row_start = row_indices[row]; 
          unsigned int row_stop  = row_indices[row + 1];
          for (unsigned int entry_index = row_start + threadIdx.x; entry_index < row_stop; ++entry_index) 
          {
            unsigned int col_index = column_indices[entry_index];
            if (col_index < row)
              vector[col_index] -= result_entry * elements[entry_index]; 
          }
          
          __syncthreads();
          
          if (threadIdx.x == 0)
            vector[row] = result_entry;
        } 
      }
      
      
      template <typename T>
      __global__ void csr_trans_lu_backward_kernel(
                const unsigned int * row_indices,
                const unsigned int * column_indices, 
                const T * elements,
                const T * diagonal_entries,
                      T * vector,
                unsigned int size) 
      {
        __shared__  unsigned int row_index_lookahead[256];
        __shared__  unsigned int row_index_buffer[256];
        
        unsigned int row_index;
        unsigned int col_index;
        T matrix_entry;
        unsigned int nnz = row_indices[size];
        unsigned int row_at_window_start = 0;
        unsigned int row_at_window_end = 0;
        unsigned int loop_end = ( (nnz - 1) / blockDim.x + 1) * blockDim.x;
        
        for (unsigned int i2 = threadIdx.x; i2 < loop_end; i2 += blockDim.x)
        {
          unsigned i = (loop_end - i2) - 1;
          col_index    = (i < nnz) ? column_indices[i] : 0;
          matrix_entry = (i < nnz) ? elements[i]       : 0;
          row_index_lookahead[threadIdx.x] = (row_at_window_start >= blockDim - threadIdx.x) ? row_indices[row_at_window_start - (blockDim - threadIdx.x - 1)] : 0;

          __syncthreads();
          
          if (i < nnz)
          {
            unsigned int row_index_dec = 0;
            while (row_index_lookahead[blockDim.x - row_index_dec - 1] > i)
              ++row_index_dec;
            row_index = row_at_window_start - row_index_dec;
            row_index_buffer[threadIdx.x] = row_index;
          }
          else
          {
            row_index = size+1;
            row_index_buffer[threadIdx.x] = size - 1;
          }
          
          __syncthreads();
          
          row_at_window_start = row_index_buffer[0];
          row_at_window_end   = row_index_buffer[blockDim.x - 1];
          
          //backward elimination
          for (unsigned int row = row_at_window_start; row <= row_at_window_end; ++row) 
          { 
            T result_entry = vector[row] / diagonal_entries[row];
            vector[row] = result_entry;
            
            if ( (row_index == row) && (col_index < row) )
              vector[col_index] -= result_entry * matrix_entry; 

            __syncthreads();
          }
          
          row_at_window_start = row_at_window_end;
        }
          
      }
      
      
      
      
      //
      // Coordinate Matrix
      //
      
      
      
      
      //
      // ELL Matrix
      //
      
      
      
      //
      // Hybrid Matrix
      //
      
      
      
    } // namespace opencl
  } //namespace linalg
} //namespace viennacl


#endif
