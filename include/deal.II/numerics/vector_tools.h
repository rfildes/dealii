// ---------------------------------------------------------------------
//
// Copyright (C) 1998 - 2019 by the deal.II authors
//
// This file is part of the deal.II library.
//
// The deal.II library is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE.md at
// the top level directory of deal.II.
//
// ---------------------------------------------------------------------

#ifndef dealii_vector_tools_h
#define dealii_vector_tools_h


#include <deal.II/base/config.h>

#include <deal.II/base/exceptions.h>
#include <deal.II/base/function.h>
#include <deal.II/base/patterns.h>
#include <deal.II/base/point.h>
#include <deal.II/base/quadrature_lib.h>

#include <deal.II/dofs/deprecated_function_map.h>
#include <deal.II/dofs/dof_handler.h>

#include <deal.II/hp/dof_handler.h>
#include <deal.II/hp/mapping_collection.h>

#include <functional>
#include <map>
#include <set>
#include <vector>

DEAL_II_NAMESPACE_OPEN

template <int dim, typename RangeNumberType>
class Function;
template <int dim>
class Quadrature;
template <int dim>
class QGauss;
template <int dim, typename number, typename VectorizedArrayType>
class MatrixFree;

template <typename number>
class Vector;
template <typename number>
class FullMatrix;
template <int dim, int spacedim>
class Mapping;
template <typename gridtype>
class InterGridMap;
namespace hp
{
  template <int dim>
  class QCollection;
}
template <typename number>
class AffineConstraints;


// TODO: Move documentation of functions to the functions!

/**
 * Provide a namespace which offers some operations on vectors. Among these
 * are assembling of standard vectors, integration of the difference of a
 * finite element solution and a continuous function, interpolations and
 * projections of continuous functions to the finite element space and other
 * operations.
 *
 * @note There exist two versions of almost all functions, one that takes an
 * explicit Mapping argument and one that does not. The second one generally
 * calls the first with an implicit $Q_1$ argument (i.e., with an argument of
 * kind MappingQGeneric(1)). If your intend your code to use a different
 * mapping than a (bi-/tri-)linear one, then you need to call the functions
 * <b>with</b> mapping argument should be used.
 *
 *
 * <h3>Description of operations</h3>
 *
 * This collection of methods offers the following operations:
 * <ul>
 * <li> Interpolation: assign each degree of freedom in the vector to be the
 * value of the function given as argument. This is identical to saying that
 * the resulting finite element function (which is isomorphic to the output
 * vector) has exact function values in all support points of trial functions.
 * The support point of a trial function is the point where its value equals
 * one, e.g. for linear trial functions the support points are four corners of
 * an element. This function therefore relies on the assumption that a finite
 * element is used for which the degrees of freedom are function values
 * (Lagrange elements) rather than gradients, normal derivatives, second
 * derivatives, etc (Hermite elements, quintic Argyris element, etc.).
 *
 * It seems inevitable that some values of the vector to be created are set
 * twice or even more than that. The reason is that we have to loop over all
 * cells and get the function values for each of the trial functions located
 * thereon. This applies also to the functions located on faces and corners
 * which we thus visit more than once. While setting the value in the vector
 * is not an expensive operation, the evaluation of the given function may be,
 * taking into account that a virtual function has to be called.
 *
 * <li> Projection: compute the <i>L</i><sup>2</sup>-projection of the given
 * function onto the finite element space, i.e. if <i>f</i> is the function to
 * be projected, compute <i>f<sub>h</sub></i> in <i>V<sub>h</sub></i> such
 * that
 * (<i>f<sub>h</sub></i>,<i>v<sub>h</sub></i>)=(<i>f</i>,<i>v<sub>h</sub></i>)
 * for all discrete test functions <i>v<sub>h</sub></i>. This is done through
 * the solution of the linear system of equations <i> M v = f</i> where
 * <i>M</i> is the mass matrix $m_{ij} = \int_\Omega \phi_i(x) \phi_j(x) dx$
 * and $f_i = \int_\Omega f(x) \phi_i(x) dx$. The solution vector $v$ then is
 * the nodal representation of the projection <i>f<sub>h</sub></i>. The
 * project() functions are used in the step-21 and step-23 tutorial programs.
 *
 * In order to get proper results, it be may necessary to treat boundary
 * conditions right. Below are listed some cases where this may be needed.  If
 * needed, this is done by <i>L</i><sup>2</sup>-projection of the trace of the
 * given function onto the finite element space restricted to the boundary of
 * the domain, then taking this information and using it to eliminate the
 * boundary nodes from the mass matrix of the whole domain, using the
 * MatrixTools::apply_boundary_values() function. The projection of the trace
 * of the function to the boundary is done with the
 * VectorTools::project_boundary_values() (see below) function, which is
 * called with a map of boundary functions
 * std::map<types::boundary_id, const Function<spacedim,number>*> in which all
 * boundary indicators from zero to numbers::internal_face_boundary_id-1
 * (numbers::internal_face_boundary_id is used for other purposes, see the
 * Triangulation class documentation) point to the function to be projected.
 * The projection to the boundary takes place using a second quadrature
 * formula on the boundary given to the project() function. The first
 * quadrature formula is used to compute the right hand side and for numerical
 * quadrature of the mass matrix.
 *
 * The projection of the boundary values first, then eliminating them from the
 * global system of equations is not needed usually. It may be necessary if
 * you want to enforce special restrictions on the boundary values of the
 * projected function, for example in time dependent problems: you may want to
 * project the initial values but need consistency with the boundary values
 * for later times. Since the latter are projected onto the boundary in each
 * time step, it is necessary that we also project the boundary values of the
 * initial values, before projecting them to the whole domain.
 *
 * Obviously, the results of the two schemes for projection are different.
 * Usually, when projecting to the boundary first, the
 * <i>L</i><sup>2</sup>-norm of the difference between original function and
 * projection over the whole domain will be larger (factors of five have been
 * observed) while the <i>L</i><sup>2</sup>-norm of the error integrated over
 * the boundary should of course be less. The reverse should also hold if no
 * projection to the boundary is performed.
 *
 * The selection whether the projection to the boundary first is needed is
 * done with the <tt>project_to_boundary_first</tt> flag passed to the
 * function.  If @p false is given, the additional quadrature formula for
 * faces is ignored.
 *
 * You should be aware of the fact that if no projection to the boundary is
 * requested, a function with zero boundary values may not have zero boundary
 * values after projection. There is a flag for this especially important
 * case, which tells the function to enforce zero boundary values on the
 * respective boundary parts. Since enforced zero boundary values could also
 * have been reached through projection, but are more economically obtain
 * using other methods, the @p project_to_boundary_first flag is ignored if
 * the @p enforce_zero_boundary flag is set.
 *
 * The solution of the linear system is presently done using a simple CG
 * method without preconditioning and without multigrid. This is clearly not
 * too efficient, but sufficient in many cases and simple to implement. This
 * detail may change in the future.
 *
 * <li> Creation of right hand side vectors: The create_right_hand_side()
 * function computes the vector $f_i = \int_\Omega f(x) \phi_i(x) dx$. This is
 * the same as what the <tt>MatrixCreator::create_*</tt> functions which take
 * a right hand side do, but without assembling a matrix.
 *
 * <li> Creation of right hand side vectors for point sources: The
 * create_point_source_vector() function computes the vector $f_i =
 * \int_\Omega \delta(x-x_0) \phi_i(x) dx$.
 *
 * <li> Creation of boundary right hand side vectors: The
 * create_boundary_right_hand_side() function computes the vector $f_i =
 * \int_{\partial\Omega} g(x) \phi_i(x) dx$. This is the right hand side
 * contribution of boundary forces when having inhomogeneous Neumann boundary
 * values in Laplace's equation or other second order operators. This function
 * also takes an optional argument denoting over which parts of the boundary
 * the integration shall extend. If the default argument is used, it is
 * applied to all boundaries.
 *
 * <li> Interpolation of boundary values: The
 * MatrixTools::apply_boundary_values() function takes a list of boundary
 * nodes and their values. You can get such a list by interpolation of a
 * boundary function using the interpolate_boundary_values() function. To use
 * it, you have to specify a list of pairs of boundary indicators (of type
 * <tt>types::boundary_id</tt>; see the section in the documentation of the
 * Triangulation class for more details) and the according functions denoting
 * the Dirichlet boundary values of the nodes on boundary faces with this
 * boundary indicator.
 *
 * Usually, all other boundary conditions, such as inhomogeneous Neumann
 * values or mixed boundary conditions are handled in the weak formulation. No
 * attempt is made to include these into the process of matrix and vector
 * assembly therefore.
 *
 * Within this function, boundary values are interpolated, i.e. a node is
 * given the point value of the boundary function. In some cases, it may be
 * necessary to use the L2-projection of the boundary function or any other
 * method. For this purpose we refer to the project_boundary_values() function
 * below.
 *
 * You should be aware that the boundary function may be evaluated at nodes on
 * the interior of faces. These, however, need not be on the true boundary,
 * but rather are on the approximation of the boundary represented by the
 * mapping of the unit cell to the real cell. Since this mapping will in most
 * cases not be the exact one at the face, the boundary function is evaluated
 * at points which are not on the boundary and you should make sure that the
 * returned values are reasonable in some sense anyway.
 *
 * In 1d the situation is a bit different since there faces (i.e. vertices)
 * have no boundary indicator. It is assumed that if the boundary indicator
 * zero is given in the list of boundary functions, the left boundary point is
 * to be interpolated while the right boundary point is associated with the
 * boundary index 1 in the map. The respective boundary functions are then
 * evaluated at the place of the respective boundary point.
 *
 * <li> Projection of boundary values: The project_boundary_values() function
 * acts similar to the interpolate_boundary_values() function, apart from the
 * fact that it does not get the nodal values of boundary nodes by
 * interpolation but rather through the <i>L</i><sup>2</sup>-projection of the
 * trace of the function to the boundary.
 *
 * The projection takes place on all boundary parts with boundary indicators
 * listed in the map (std::map<types::boundary_id, const
 * Function<spacedim,number>*>) of boundary functions. These boundary parts may
 * or may not be continuous. For these boundary parts, the mass matrix is
 * assembled using the MatrixTools::create_boundary_mass_matrix() function, as
 * well as the appropriate right hand side. Then the resulting system of
 * equations is solved using a simple CG method (without preconditioning), which
 * is in most cases sufficient for the present purpose.
 *
 * <li> Computing errors: The function integrate_difference() performs the
 * calculation of the error between a given (continuous) reference function
 * and the finite element solution in different norms. The integration is
 * performed using a given quadrature formula and assumes that the given
 * finite element objects equals that used for the computation of the
 * solution.
 *
 * The result is stored in a vector (named @p difference), where each entry
 * equals the given norm of the difference on a cell. The order of entries is
 * the same as a @p cell_iterator takes when started with @p begin_active and
 * promoted with the <tt>++</tt> operator.
 *
 * This data, one number per active cell, can be used to generate graphical
 * output by directly passing it to the DataOut class through the
 * DataOut::add_data_vector function. Alternatively, the global error can be
 * computed using VectorTools::compute_global_error(). Finally, the output per
 * cell from VectorTools::integrate_difference() can be interpolated to the
 * nodal points of a finite element field using the
 * DoFTools::distribute_cell_to_dof_vector function.
 *
 * Presently, there is the possibility to compute the following values from
 * the difference, on each cell: @p mean, @p L1_norm, @p L2_norm, @p
 * Linfty_norm, @p H1_seminorm and @p H1_norm, see VectorTools::NormType. For
 * the mean difference value, the reference function minus the numerical
 * solution is computed, not the other way round.
 *
 * The infinity norm of the difference on a given cell returns the maximum
 * absolute value of the difference at the quadrature points given by the
 * quadrature formula parameter. This will in some cases not be too good an
 * approximation, since for example the Gauss quadrature formulae do not
 * evaluate the difference at the end or corner points of the cells. You may
 * want to choose a quadrature formula with more quadrature points or one with
 * another distribution of the quadrature points in this case. You should also
 * take into account the superconvergence properties of finite elements in
 * some points: for example in 1D, the standard finite element method is a
 * collocation method and should return the exact value at nodal points.
 * Therefore, the trapezoidal rule should always return a vanishing L-infinity
 * error. Conversely, in 2D the maximum L-infinity error should be located at
 * the vertices or at the center of the cell, which would make it plausible to
 * use the Simpson quadrature rule. On the other hand, there may be
 * superconvergence at Gauss integration points. These examples are not
 * intended as a rule of thumb, rather they are thought to illustrate that the
 * use of the wrong quadrature formula may show a significantly wrong result
 * and care should be taken to chose the right formula.
 *
 * The <i>H</i><sup>1</sup> seminorm is the <i>L</i><sup>2</sup> norm of the
 * gradient of the difference. The square of the full <i>H</i><sup>1</sup>
 * norm is the sum of the square of seminorm and the square of the
 * <i>L</i><sup>2</sup> norm.
 *
 * To get the global <i>L<sup>1</sup></i> error, you have to sum up the
 * entries in @p difference, e.g. using Vector::l1_norm() function.  For the
 * global <i>L</i><sup>2</sup> difference, you have to sum up the squares of
 * the entries and take the root of the sum, e.g. using Vector::l2_norm().
 * These two operations represent the <i>l</i><sub>1</sub> and
 * <i>l</i><sub>2</sub> norms of the vectors, but you need not take the
 * absolute value of each entry, since the cellwise norms are already
 * positive.
 *
 * To get the global mean difference, simply sum up the elements as above. To
 * get the $L_\infty$ norm, take the maximum of the vector elements, e.g.
 * using the Vector::linfty_norm() function.
 *
 * For the global <i>H</i><sup>1</sup> norm and seminorm, the same rule
 * applies as for the <i>L</i><sup>2</sup> norm: compute the
 * <i>l</i><sub>2</sub> norm of the cell error vector.
 *
 * Note that, in the codimension one case, if you ask for a norm that requires
 * the computation of a gradient, then the provided function is automatically
 * projected along the curve, and the difference is only computed on the
 * tangential part of the gradient, since no information is available on the
 * normal component of the gradient anyway.
 * </ul>
 *
 * All functions use the finite element given to the DoFHandler object the
 * last time that the degrees of freedom were distributed over the
 * triangulation. Also, if access to an object describing the exact form of
 * the boundary is needed, the pointer stored within the triangulation object
 * is accessed.
 *
 * @note Instantiations for this template are provided for some vector types,
 * in particular <code>Vector&lt;float&gt;, Vector&lt;double&gt;,
 * BlockVector&lt;float&gt;, BlockVector&lt;double&gt;</code>; others can be
 * generated in application code (see the section on
 * @ref Instantiations
 * in the manual).
 *
 * @ingroup numerics
 * @author Wolfgang Bangerth, Ralf Hartmann, Guido Kanschat, 1998, 1999, 2000,
 * 2001
 */
namespace VectorTools
{
  /**
   * Denote which norm/integral is to be computed by the
   * integrate_difference() function on each cell and compute_global_error()
   * for the whole domain.
   * Let $f:\Omega \rightarrow \mathbb{R}^c$ be a finite element function
   * with $c$ components where component $c$ is denoted by $f_c$ and $\hat{f}$
   * be the reference function (the @p fe_function and @p exact_solution
   * arguments to integrate_difference()). Let $e_c = \hat{f}_c - f_c$
   * be the difference or error between the two. Further,
   * let  $w:\Omega \rightarrow \mathbb{R}^c$ be the @p weight function of integrate_difference(), which is
   * assumed to be equal to one if not supplied. Finally, let $p$ be the
   * @p exponent argument (for $L_p$-norms).
   *
   * In the following,we denote by $E_K$ the local error computed by
   * integrate_difference() on cell $K$, whereas $E$ is the global error
   * computed by compute_global_error(). Note that integrals are
   * approximated by quadrature in the usual way:
   * @f[
   * \int_A f(x) dx \approx \sum_q f(x_q) \omega_q.
   * @f]
   * Similarly for suprema over a cell $T$:
   * @f[
   * \sup_{x\in T} |f(x)| dx \approx \max_q |f(x_q)|.
   * @f]
   */
  enum NormType
  {
    /**
     * The function or difference of functions is integrated on each cell $K$:
     * @f[
     *   E_K
     * = \int_K \sum_c (\hat{f}_c - f_c) \, w_c
     * = \int_K \sum_c e_c \, w_c
     * @f]
     * and summed up to get
     * @f[
     *   E = \sum_K E_K
     *     = \int_\Omega \sum_c (\hat{f}_c - f_c) \, w_c
     * @f]
     * or, for $w \equiv 1$:
     * @f[
     *   E = \int_\Omega (\hat{f} - f)
     *     = \int_\Omega e.
     * @f]
     *
     * Note: This differs from what is typically known as
     * the mean of a function by a factor of $\frac{1}{|\Omega|}$. To
     * compute the mean you can also use compute_mean_value(). Finally,
     * pay attention to the sign: if $\hat{f}=0$, this will compute the
     * negative of the mean of $f$.
     */
    mean,

    /**
     * The absolute value of the function is integrated:
     * @f[
     *   E_K = \int_K \sum_c |e_c| \, w_c
     * @f]
     * and
     * @f[
     *   E = \sum_K E_K = \int_\Omega \sum_c |e_c| w_c,
     * @f]
     * or, for $w \equiv 1$:
     * @f[
     *   E  = \| e \|_{L^1}.
     * @f]
     */
    L1_norm,

    /**
     * The square of the function is integrated and the square root of the
     * result is computed on each cell:
     * @f[
     *   E_K = \sqrt{ \int_K \sum_c e_c^2 \, w_c }
     * @f]
     * and
     * @f[
     *   E = \sqrt{\sum_K E_K^2} = \sqrt{ \int_\Omega  \sum_c e_c^2 \, w_c }
     * @f]
     * or, for $w \equiv 1$:
     * @f[
     *   E = \sqrt{ \int_\Omega e^2 }
     *     = \| e \|_{L^2}
     * @f]
     */
    L2_norm,

    /**
     * The absolute value to the $p$-th power is integrated and the $p$-th
     * root is computed on each cell. The exponent $p$ is the @p
     * exponent argument of integrate_difference() and compute_mean_value():
     * @f[
     *   E_K = \left( \int_K \sum_c |e_c|^p \, w_c \right)^{1/p}
     * @f]
     * and
     * @f[
     *   E = \left( \sum_K E_K^p \right)^{1/p}
     * @f]
     * or, for $w \equiv 1$:
     * @f[
     *   E = \| e \|_{L^p}.
     * @f]
     */
    Lp_norm,

    /**
     * The maximum absolute value of the function:
     * @f[
     *   E_K = \sup_K \max_c |e_c| \, w_c
     * @f]
     * and
     * @f[
     *   E = \max_K E_K
     * = \sup_\Omega \max_c |e_c| \, w_c
     * @f]
     * or, for $w \equiv 1$:
     * @f[
     *   E  = \sup_\Omega \|e\|_\infty = \| e \|_{L^\infty}.
     * @f]
     */
    Linfty_norm,

    /**
     * #L2_norm of the gradient:
     * @f[
     *   E_K = \sqrt{ \int_K \sum_c (\nabla e_c)^2 \, w_c }
     * @f]
     * and
     * @f[
     *   E = \sqrt{\sum_K E_K^2} = \sqrt{ \int_\Omega \sum_c (\nabla e_c)^2 \,
     * w_c }
     * @f]
     * or, for $w \equiv 1$:
     * @f[
     *   E = \| \nabla e \|_{L^2}.
     * @f]
     */
    H1_seminorm,

    /**
     * #L2_norm of the divergence of a vector field. The function $f$ is
     * expected to have $c \geq \text{dim}$ components and the first @p dim
     * will be used to compute the divergence:
     * @f[
     *   E_K = \sqrt{ \int_K \left( \sum_c \frac{\partial e_c}{\partial x_c} \,
     * \sqrt{w_c} \right)^2 }
     * @f]
     * and
     * @f[
     *   E = \sqrt{\sum_K E_K^2}
     *     = \sqrt{ \int_\Omega \left( \sum_c \frac{\partial e_c}{\partial x_c}
     * \, \sqrt{w_c} \right)^2  }
     * @f]
     * or, for $w \equiv 1$:
     * @f[
     *   E = \| \nabla \cdot e \|_{L^2}.
     * @f]
     */
    Hdiv_seminorm,

    /**
     * The square of this norm is the square of the #L2_norm plus the square
     * of the #H1_seminorm:
     * @f[
     *   E_K = \sqrt{ \int_K \sum_c (e_c^2 + (\nabla e_c)^2) \, w_c }
     * @f]
     * and
     * @f[
     *   E = \sqrt{\sum_K E_K^2} = \sqrt{ \int_\Omega \sum_c (e_c^2 + (\nabla
     * e_c)^2) \, w_c }
     * @f]
     * or, for $w \equiv 1$:
     * @f[
     *   E = \left( \| e \|_{L^2}^2 + \| \nabla e \|_{L^2}^2 \right)^{1/2}.
     * @f]
     */
    H1_norm,

    /**
     * #Lp_norm of the gradient:
     * @f[
     *   E_K = \left( \int_K \sum_c |\nabla e_c|^p \, w_c \right)^{1/p}
     * @f]
     * and
     * @f[
     *   E = \left( \sum_K E_K^p \right)^{1/p}
     *     = \left( \int_\Omega \sum_c |\nabla e_c|^p \, w_c \right)^{1/p}
     * @f]
     * or, for $w \equiv 1$:
     * @f[
     *   E = \| \nabla e \|_{L^p}.
     * @f]
     */
    W1p_seminorm,

    /**
     * The same as the #H1_norm but using <i>L<sup>p</sup></i>:
     * @f[
     *   E_K = \left( \int_K \sum_c (|e_c|^p + |\nabla e_c|^p) \, w_c
     * \right)^{1/p}
     * @f]
     * and
     * @f[
     *   E = \left( \sum_K E_K^p \right)^{1/p}
     *     = \left( \int_\Omega \sum_c (|e_c|^p + |\nabla e_c|^p) \, w_c
     * \right)^{1/p}
     * @f]
     * or, for $w \equiv 1$:
     * @f[
     *   E = \left( \| e \|_{L^p}^p + \| \nabla e \|_{L^p}^p \right)^{1/p}.
     * @f]
     */
    W1p_norm,

    /**
     * #Linfty_norm of the gradient:
     * @f[
     *   E_K = \sup_K \max_c |\nabla e_c| \, w_c
     * @f]
     * and
     * @f[
     *   E = \max_K E_K
     *     = \sup_\Omega \max_c |\nabla e_c| \, w_c
     * @f]
     * or, for $w \equiv 1$:
     * @f[
     *   E = \| \nabla e \|_{L^\infty}.
     * @f]
     *
     */
    W1infty_seminorm,

    /**
     * The sum of #Linfty_norm and #W1infty_seminorm:
     * @f[
     *   E_K = \sup_K \max_c |e_c| \, w_c + \sup_K \max_c |\nabla e_c| \, w_c.
     * @f]
     * The global norm is not implemented in compute_global_error(),
     * because it is impossible to compute the sum of the global
     * norms from the values $E_K$. As a work-around, you can compute the
     * global #Linfty_norm and #W1infty_seminorm separately and then add them
     * to get (with $w \equiv 1$):
     * @f[
     *   E = \| e \|_{L^\infty} + \| \nabla e \|_{L^\infty}.
     * @f]
     */
    W1infty_norm

  };
  /**
   * @name Interpolation and projection
   */
  //@{



  /**
   * Compute the interpolation of @p function at the support points to the
   * finite element space described by the Triangulation and FiniteElement
   * object with which the given DoFHandler argument is initialized. It is
   * assumed that the number of components of @p function matches that of the
   * finite element used by @p dof.
   *
   * Note that you may have to call <tt>hanging_nodes.distribute(vec)</tt>
   * with the hanging nodes from space @p dof afterwards, to make the result
   * continuous again.
   *
   * The template argument <code>DoFHandlerType</code> may either be of type
   * DoFHandler or hp::DoFHandler.
   *
   * See the general documentation of this namespace for further information.
   *
   * @todo The @p mapping argument should be replaced by a
   * hp::MappingCollection in case of a hp::DoFHandler.
   */
  template <int dim,
            int spacedim,
            typename VectorType,
            template <int, int> class DoFHandlerType>
  void
  interpolate(
    const Mapping<dim, spacedim> &                             mapping,
    const DoFHandlerType<dim, spacedim> &                      dof,
    const Function<spacedim, typename VectorType::value_type> &function,
    VectorType &                                               vec,
    const ComponentMask &component_mask = ComponentMask());

  /**
   * Call the @p interpolate() function above with
   * <tt>mapping=MappingQGeneric1@<dim>@()</tt>.
   */
  template <int dim,
            int spacedim,
            typename VectorType,
            template <int, int> class DoFHandlerType>
  void
  interpolate(
    const DoFHandlerType<dim, spacedim> &                      dof,
    const Function<spacedim, typename VectorType::value_type> &function,
    VectorType &                                               vec,
    const ComponentMask &component_mask = ComponentMask());

  /**
   * Interpolate different finite element spaces. The interpolation of vector
   * @p data_1 is executed from the FE space represented by @p dof_1 to the
   * vector @p data_2 on FE space @p dof_2. The interpolation on each cell is
   * represented by the matrix @p transfer. Curved boundaries are neglected so
   * far.
   *
   * Note that you may have to call <tt>hanging_nodes.distribute(data_2)</tt>
   * with the hanging nodes from space @p dof_2 afterwards, to make the result
   * continuous again.
   *
   * @note Instantiations for this template are provided for some vector types
   * (see the general documentation of the namespace), but only the same
   * vector for InVector and OutVector. Other combinations must be
   * instantiated by hand.
   */
  template <int dim, class InVector, class OutVector, int spacedim>
  void
  interpolate(const DoFHandler<dim, spacedim> &dof_1,
              const DoFHandler<dim, spacedim> &dof_2,
              const FullMatrix<double> &       transfer,
              const InVector &                 data_1,
              OutVector &                      data_2);

  /**
   * This function is a kind of generalization or modification of the very
   * first interpolate() function in the series. It interpolates a set of
   * functions onto the finite element space defined by the DoFHandler argument,
   * where the determination which function to use on each cell is made
   * based on the material id (see
   * @ref GlossMaterialId)
   * of each cell.
   *
   * @param[in] mapping        The mapping to use to determine the location of
   *   support points at which the functions are to be evaluated.
   * @param[in] dof_handler    DoFHandler initialized with Triangulation and
   *   FiniteElement objects and that defines the finite element space.
   * @param[in] function_map   A std::map reflecting the correspondence between
   *   material ids on those cells on which something should be interpolated,
   *   and the functions to be interpolated onto the finite element space.
   * @param[out] dst           The global finie element vector holding the
   *   output of the interpolated values.
   * @param[in] component_mask A mask of components that shall be interpolated.
   *
   * @note If the algorithm encounters a cell whose material id is not listed
   * in the given @p function_map, then @p dst will not be updated in the
   * respective degrees of freedom of the output vector. For example, if
   * @p dst was initialized to zero, then those zeros which correspond to
   * the missed material ids will still remain in @p dst after calling
   * this function.
   *
   * @note Degrees of freedom located on faces between cells of different
   * material ids will get their value by that cell which was called last in
   * the respective loop over cells implemented in this function. Since the
   * order of cells is somewhat arbitrary, you cannot control it. However, if
   * you want to have control over the order in which cells are visited, let us
   * take a
   * look at the following example: Let @p u be a variable of interest which
   * is approximated by some CG finite element. Let @p 0, @p 1 and @p 2 be
   * material ids of cells on the triangulation. Let 0: 0.0, 1: 1.0, 2: 2.0 be
   * the whole @p function_map that you want to pass to this function, where
   * @p key is a material id and @p value is a value of @p u. By using the
   * whole @p function_map you do not really know which values will be
   * assigned to the face DoFs. On the other hand, if you split the whole @p
   * function_map into three smaller independent objects 0: 0.0 and 1: 1.0 and
   * 2: 2.0 and make three distinct calls of this function passing each of
   * these objects separately (the order depends on what you want to get
   * between cells), then each subsequent call will rewrite the intercell @p
   * dofs of the previous one.
   *
   * @author Valentin Zingan, 2013
   */
  template <int dim,
            int spacedim,
            typename VectorType,
            template <int, int> class DoFHandlerType>
  void
  interpolate_based_on_material_id(
    const Mapping<dim, spacedim> &       mapping,
    const DoFHandlerType<dim, spacedim> &dof_handler,
    const std::map<types::material_id,
                   const Function<spacedim, typename VectorType::value_type> *>
      &                  function_map,
    VectorType &         dst,
    const ComponentMask &component_mask = ComponentMask());

  /**
   * Compute the interpolation of a @p dof1-function @p u1 to a @p dof2-function
   * @p u2, where @p dof1 and @p dof2 represent different triangulations with
   * a common coarse grid.
   *
   * dof1 and dof2 need to have the same finite element discretization.
   *
   * Note that for continuous elements on grids with hanging nodes (i.e.
   * locally refined grids) this function does not give the expected output.
   * Indeed, the resulting output vector does not necessarily respect
   * continuity requirements at hanging nodes, due to local cellwise
   * interpolation.
   *
   * For this case (continuous elements on grids with hanging nodes), please
   * use the interpolate_to_different_mesh function with an additional
   * AffineConstraints argument, see below, or make the field conforming
   * yourself by calling the @p AffineConstraints::distribute function of your
   * hanging node constraints object.
   *
   * @note: This function works with parallel::distributed::Triangulation, but
   * only if the parallel partitioning is the same for both meshes (see the
   * parallel::distributed::Triangulation<dim>::no_automatic_repartitioning
   * flag).
   */
  template <int dim,
            int spacedim,
            typename VectorType,
            template <int, int> class DoFHandlerType>
  void
  interpolate_to_different_mesh(const DoFHandlerType<dim, spacedim> &dof1,
                                const VectorType &                   u1,
                                const DoFHandlerType<dim, spacedim> &dof2,
                                VectorType &                         u2);

  /**
   * Compute the interpolation of a @p dof1-function @p u1 to a @p dof2-function
   * @p u2, where @p dof1 and @p dof2 represent different triangulations with
   * a common coarse grid.
   *
   * dof1 and dof2 need to have the same finite element discretization.
   *
   * @p constraints is a hanging node constraints object corresponding to @p
   * dof2. This object is particularly important when interpolating onto
   * continuous elements on grids with hanging nodes (locally refined grids):
   * Without it - due to cellwise interpolation - the resulting output vector
   * does not necessarily respect continuity requirements at hanging nodes.
   */
  template <int dim,
            int spacedim,
            typename VectorType,
            template <int, int> class DoFHandlerType>
  void
  interpolate_to_different_mesh(
    const DoFHandlerType<dim, spacedim> &                     dof1,
    const VectorType &                                        u1,
    const DoFHandlerType<dim, spacedim> &                     dof2,
    const AffineConstraints<typename VectorType::value_type> &constraints,
    VectorType &                                              u2);

  /**
   * The same function as above, but takes an InterGridMap object directly as
   * a parameter. Useful for interpolating several vectors at the same time.
   *
   * @p intergridmap has to be initialized via InterGridMap::make_mapping
   * pointing from a source DoFHandler to a destination DoFHandler.
   */
  template <int dim,
            int spacedim,
            typename VectorType,
            template <int, int> class DoFHandlerType>
  void
  interpolate_to_different_mesh(
    const InterGridMap<DoFHandlerType<dim, spacedim>> &       intergridmap,
    const VectorType &                                        u1,
    const AffineConstraints<typename VectorType::value_type> &constraints,
    VectorType &                                              u2);

  /**
   * Compute the projection of @p function to the finite element space. In other
   * words, given a function $f(\mathbf x)$, the current function computes a
   * finite element function $f_h(\mathbf x)=\sum_j F_j \varphi_j(\mathbf x)$
   * characterized by the (output) vector of nodal values $F$ that satisfies
   * the equation
   * @f{align*}{
   *   (\varphi_i, f_h)_\Omega = (\varphi_i,f)_\Omega
   * @f}
   * for all test functions $\varphi_i$. This requires solving a linear system
   * involving the mass matrix since the equation above is equivalent to
   * the linear system
   * @f{align*}{
   *   \sum_j (\varphi_i, \varphi_j)_\Omega F_j = (\varphi_i,f)_\Omega
   * @f}
   * which can also be written as $MF = \Phi$ with
   * $M_{ij} = (\varphi_i, \varphi_j)_\Omega$ and
   * $\Phi_i = (\varphi_i,f)_\Omega$.
   *
   * By default, no boundary values for $f_h$ are needed nor
   * imposed, but there are optional parameters to this function that allow
   * imposing either zero boundary values or, in a first step, to project
   * the boundary values of $f$ onto the finite element space on the boundary
   * of the mesh in a similar way to above, and then using these values as the
   * imposed boundary values for $f_h$. The ordering of arguments to this
   * function is such that you need not give a second quadrature formula (of
   * type `Quadrature<dim-1>` and used for the computation of the matrix and
   * right hand side for the projection of boundary values) if you
   * don't want to project to the boundary first, but that you must if you want
   * to do so.
   *
   * A MatrixFree implementation is used if the following conditions are met:
   * - @p enforce_zero_boundary is false,
   * - @p project_to_boundary_first is false,
   * - the FiniteElement is supported by the MatrixFree class,
   * - the FiniteElement has less than five components
   * - the degree of the FiniteElement is less than nine.
   * - dim==spacedim
   *
   * In this case, this function performs numerical quadrature using the given
   * quadrature formula for integration of the right hand side $\Phi_i$ while a
   * QGauss(fe_degree+2) object is used for the mass operator. You should
   * therefore make sure that the given quadrature formula is sufficiently
   * accurate for creating the right-hand side.
   *
   * Otherwise, only serial Triangulations are supported and the mass matrix
   * is assembled using MatrixTools::create_mass_matrix. The given
   * quadrature rule is then used for both the matrix and the right-hand side.
   * You should therefore make sure that the given quadrature formula is also
   * sufficient for creating the mass matrix. In particular, the degree of the
   * quadrature formula must be sufficiently high to ensure that the mass
   * matrix is invertible. For example, if you are using a FE_Q(k) element,
   * then the integrand of the matrix entries $M_{ij}$ is of polynomial
   * degree $2k$ in each variable, and you need a Gauss quadrature formula
   * with $k+1$ points in each coordinate direction to ensure that $M$
   * is invertible.
   *
   * See the general documentation of this namespace for further information.
   *
   * In 1d, the default value of the boundary quadrature formula is an invalid
   * object since integration on the boundary doesn't happen in 1d.
   *
   * @param[in] mapping The mapping object to use.
   * @param[in] dof The DoFHandler the describes the finite element space to
   * project into and that corresponds to @p vec.
   * @param[in] constraints Constraints to be used when assembling the mass
   * matrix, typically needed when you have hanging nodes.
   * @param[in] quadrature The quadrature formula to be used for assembling the
   * mass matrix.
   * @param[in] function The function to project into the finite element space.
   * @param[out] vec The output vector where the projected function will be
   * stored in. This vector is required to be already initialized and must not
   * have ghost elements.
   * @param[in] enforce_zero_boundary If true, @p vec will have zero boundary
   * conditions.
   * @param[in] q_boundary Quadrature rule to be used if @p project_to_boundary_first
   * is true.
   * @param[in] project_to_boundary_first If true, perform a projection on the
   * boundary before projecting the interior of the function.
   */
  template <int dim, typename VectorType, int spacedim>
  void
  project(const Mapping<dim, spacedim> &                            mapping,
          const DoFHandler<dim, spacedim> &                         dof,
          const AffineConstraints<typename VectorType::value_type> &constraints,
          const Quadrature<dim> &                                   quadrature,
          const Function<spacedim, typename VectorType::value_type> &function,
          VectorType &                                               vec,
          const bool                 enforce_zero_boundary     = false,
          const Quadrature<dim - 1> &q_boundary                = (dim > 1 ?
                                                     QGauss<dim - 1>(2) :
                                                     Quadrature<dim - 1>(0)),
          const bool                 project_to_boundary_first = false);

  /**
   * Call the project() function above, with
   * <tt>mapping=MappingQGeneric@<dim@>(1)</tt>.
   */
  template <int dim, typename VectorType, int spacedim>
  void
  project(const DoFHandler<dim, spacedim> &                         dof,
          const AffineConstraints<typename VectorType::value_type> &constraints,
          const Quadrature<dim> &                                   quadrature,
          const Function<spacedim, typename VectorType::value_type> &function,
          VectorType &                                               vec,
          const bool                 enforce_zero_boundary     = false,
          const Quadrature<dim - 1> &q_boundary                = (dim > 1 ?
                                                     QGauss<dim - 1>(2) :
                                                     Quadrature<dim - 1>(0)),
          const bool                 project_to_boundary_first = false);

  /**
   * Same as above, but for arguments of type hp::DoFHandler, hp::QCollection,
   * and hp::MappingCollection.
   */
  template <int dim, typename VectorType, int spacedim>
  void
  project(const hp::MappingCollection<dim, spacedim> &              mapping,
          const hp::DoFHandler<dim, spacedim> &                     dof,
          const AffineConstraints<typename VectorType::value_type> &constraints,
          const hp::QCollection<dim> &                              quadrature,
          const Function<spacedim, typename VectorType::value_type> &function,
          VectorType &                                               vec,
          const bool                      enforce_zero_boundary = false,
          const hp::QCollection<dim - 1> &q_boundary = hp::QCollection<dim - 1>(
            dim > 1 ? QGauss<dim - 1>(2) : Quadrature<dim - 1>(0)),
          const bool project_to_boundary_first = false);

  /**
   * Call the project() function above, with a collection of $Q_1$ mapping
   * objects, i.e., with hp::StaticMappingQ1::mapping_collection.
   */
  template <int dim, typename VectorType, int spacedim>
  void
  project(const hp::DoFHandler<dim, spacedim> &                     dof,
          const AffineConstraints<typename VectorType::value_type> &constraints,
          const hp::QCollection<dim> &                              quadrature,
          const Function<spacedim, typename VectorType::value_type> &function,
          VectorType &                                               vec,
          const bool                      enforce_zero_boundary = false,
          const hp::QCollection<dim - 1> &q_boundary = hp::QCollection<dim - 1>(
            dim > 1 ? QGauss<dim - 1>(2) : Quadrature<dim - 1>(0)),
          const bool project_to_boundary_first = false);

  /**
   * The same as above for projection of scalar-valued quadrature data.
   * The user provided function should return a value at the quadrature point
   * based on the cell iterator and quadrature number and of course should be
   * consistent with the provided @p quadrature object, which will be used
   * to assemble the right-hand-side.
   *
   * This function can be used with lambdas:
   * @code
   * VectorTools::project
   * (mapping,
   *  dof_handler,
   *  constraints,
   *  quadrature_formula,
   *  [&] (const typename DoFHandler<dim>::active_cell_iterator & cell,
   *       const unsigned int q) -> double
   *  {
   *    return qp_data.get_data(cell)[q]->density;
   *  },
   *  field);
   * @endcode
   * where <code>qp_data</code> is a CellDataStorage object, which stores
   * quadrature point data.
   */
  template <int dim, typename VectorType, int spacedim>
  void
  project(const Mapping<dim, spacedim> &                            mapping,
          const DoFHandler<dim, spacedim> &                         dof,
          const AffineConstraints<typename VectorType::value_type> &constraints,
          const Quadrature<dim> &                                   quadrature,
          const std::function<typename VectorType::value_type(
            const typename DoFHandler<dim, spacedim>::active_cell_iterator &,
            const unsigned int)> &                                  func,
          VectorType &                                              vec_result);

  /**
   * The same as above for projection of scalar-valued MatrixFree quadrature
   * data.
   * The user provided function @p func should return a VectorizedArray value
   * at the quadrature point based on the cell number and quadrature number and
   * should be consistent with the @p n_q_points_1d.
   *
   * This function can be used with lambdas:
   * @code
   * VectorTools::project
   * (matrix_free_data,
   *  constraints,
   *  3,
   *  [&] (const unsigned int cell,
   *       const unsigned int q) -> VectorizedArray<double>
   *  {
   *    return qp_data(cell,q);
   *  },
   *  field);
   * @endcode
   * where <code>qp_data</code> is a an object of type Table<2,
   * VectorizedArray<double> >, which stores quadrature point data.
   *
   * @p fe_component allow to additionally specify which component of @p data
   * to use in case it was constructed with an <code>std::vector<const
   * DoFHandler<dim>*></code>. It will be used internally in constructor of
   * FEEvaluation object.
   */
  template <int dim, typename VectorType>
  void
  project(
    std::shared_ptr<
      const MatrixFree<dim,
                       typename VectorType::value_type,
                       VectorizedArray<typename VectorType::value_type>>> data,
    const AffineConstraints<typename VectorType::value_type> &constraints,
    const unsigned int                                        n_q_points_1d,
    const std::function<VectorizedArray<typename VectorType::value_type>(
      const unsigned int,
      const unsigned int)> &                                  func,
    VectorType &                                              vec_result,
    const unsigned int                                        fe_component = 0);

  /**
   * Same as above but for <code>n_q_points_1d =
   * matrix_free.get_dof_handler().get_fe().degree+1</code>.
   */
  template <int dim, typename VectorType>
  void
  project(
    std::shared_ptr<
      const MatrixFree<dim,
                       typename VectorType::value_type,
                       VectorizedArray<typename VectorType::value_type>>> data,
    const AffineConstraints<typename VectorType::value_type> &constraints,
    const std::function<VectorizedArray<typename VectorType::value_type>(
      const unsigned int,
      const unsigned int)> &                                  func,
    VectorType &                                              vec_result,
    const unsigned int                                        fe_component = 0);

  /**
   * Compute Dirichlet boundary conditions.  This function makes up a map of
   * degrees of freedom subject to Dirichlet boundary conditions and the
   * corresponding values to be assigned to them, by interpolation around the
   * boundary. For each degree of freedom at the boundary, if its index
   * already exists in @p boundary_values then its boundary value will be
   * overwritten, otherwise a new entry with proper index and boundary value
   * for this degree of freedom will be inserted into @p boundary_values.
   *
   * The parameter @p function_map provides a list of boundary indicators to
   * be handled by this function and corresponding boundary value functions.
   * The keys of this map correspond to the number @p boundary_id of the face.
   * numbers::internal_face_boundary_id is an illegal value for this key since
   * it is reserved for interior faces. For an example of how to use this
   * argument with a non-empty map, see the step-16 tutorial program.
   *
   * The flags in the last parameter, @p component_mask denote which
   * components of the finite element space shall be interpolated. If it is
   * left as specified by the default value (i.e. an empty array), all
   * components are interpolated. If it is different from the default value,
   * it is assumed that the number of entries equals the number of components
   * in the boundary functions and the finite element, and those components in
   * the given boundary function will be used for which the respective flag
   * was set in the component mask. See also
   * @ref GlossComponentMask.
   * As an example, assume that you are solving the Stokes equations in 2d,
   * with variables $(u,v,p)$ and that you only want to interpolate boundary
   * values for the velocity, then the component mask should correspond to
   * <code>(true,true,false)</code>.
   *
   * @note Whether a component mask has been specified or not, the number of
   * components of the functions in @p function_map must match that of the
   * finite element used by @p dof. In other words, for the example above, you
   * need to provide a Function object that has 3 components (the two
   * velocities and the pressure), even though you are only interested in the
   * first two of them. interpolate_boundary_values() will then call this
   * function to obtain a vector of 3 values at each interpolation point but
   * only take the first two and discard the third. In other words, you are
   * free to return whatever you like in the third component of the vector
   * returned by Function::vector_value, but the Function object must state
   * that it has 3 components.
   *
   * If the finite element used has shape functions that are non-zero in more
   * than one component (in deal.II speak: they are non-primitive), then these
   * components can presently not be used for interpolating boundary values.
   * Thus, the elements in the component mask corresponding to the components
   * of these non-primitive shape functions must be @p false.
   *
   * See the general documentation of this namespace for more information.
   */
  template <int dim,
            int spacedim,
            template <int, int> class DoFHandlerType,
            typename number>
  void
  interpolate_boundary_values(
    const Mapping<dim, spacedim> &       mapping,
    const DoFHandlerType<dim, spacedim> &dof,
    const std::map<types::boundary_id, const Function<spacedim, number> *>
      &                                        function_map,
    std::map<types::global_dof_index, number> &boundary_values,
    const ComponentMask &component_mask = ComponentMask());

  /**
   * Like the previous function, but take a mapping collection to go with the
   * hp::DoFHandler object.
   */
  template <int dim, int spacedim, typename number>
  void
  interpolate_boundary_values(
    const hp::MappingCollection<dim, spacedim> &mapping,
    const hp::DoFHandler<dim, spacedim> &       dof,
    const std::map<types::boundary_id, const Function<spacedim, number> *>
      &                                        function_map,
    std::map<types::global_dof_index, number> &boundary_values,
    const ComponentMask &component_mask = ComponentMask());

  /**
   * Same function as above, but taking only one pair of boundary indicator
   * and corresponding boundary function. The same comments apply as for the
   * previous function, in particular about the use of the component mask and
   * the requires size of the function object.
   *
   * @see
   * @ref GlossBoundaryIndicator "Glossary entry on boundary indicators"
   */
  template <int dim,
            int spacedim,
            template <int, int> class DoFHandlerType,
            typename number>
  void
  interpolate_boundary_values(
    const Mapping<dim, spacedim> &             mapping,
    const DoFHandlerType<dim, spacedim> &      dof,
    const types::boundary_id                   boundary_component,
    const Function<spacedim, number> &         boundary_function,
    std::map<types::global_dof_index, number> &boundary_values,
    const ComponentMask &component_mask = ComponentMask());

  /**
   * Call the other interpolate_boundary_values() function, see above, with
   * <tt>mapping=MappingQGeneric@<dim,spacedim@>(1)</tt>. The same comments
   * apply as for the previous function, in particular about the use of the
   * component mask and the requires size of the function object.
   *
   * @see
   * @ref GlossBoundaryIndicator "Glossary entry on boundary indicators"
   */
  template <int dim,
            int spacedim,
            template <int, int> class DoFHandlerType,
            typename number>
  void
  interpolate_boundary_values(
    const DoFHandlerType<dim, spacedim> &      dof,
    const types::boundary_id                   boundary_component,
    const Function<spacedim, number> &         boundary_function,
    std::map<types::global_dof_index, number> &boundary_values,
    const ComponentMask &component_mask = ComponentMask());


  /**
   * Call the other interpolate_boundary_values() function, see above, with
   * <tt>mapping=MappingQGeneric@<dim,spacedim@>(1)</tt>. The same comments
   * apply as for the previous function, in particular about the use of the
   * component mask and the requires size of the function object.
   */
  template <int dim,
            int spacedim,
            template <int, int> class DoFHandlerType,
            typename number>
  void
  interpolate_boundary_values(
    const DoFHandlerType<dim, spacedim> &dof,
    const std::map<types::boundary_id, const Function<spacedim, number> *>
      &                                        function_map,
    std::map<types::global_dof_index, number> &boundary_values,
    const ComponentMask &component_mask = ComponentMask());


  /**
   * Insert the (algebraic) constraints due to Dirichlet boundary conditions
   * into a AffineConstraints @p constraints. This function identifies the
   * degrees of freedom subject to Dirichlet boundary conditions, adds them to
   * the list of constrained DoFs in @p constraints and sets the respective
   * inhomogeneity to the value interpolated around the boundary. If this
   * routine encounters a DoF that already is constrained (for instance by a
   * hanging node constraint, see below, or any other type of constraint, e.g.
   * from periodic boundary conditions), the old setting of the constraint
   * (dofs the entry is constrained to, inhomogeneities) is kept and nothing
   * happens.
   *
   * @note When combining adaptively refined meshes with hanging node
   * constraints and boundary conditions like from the current function within
   * one AffineConstraints object, the hanging node constraints should always
   * be set first, and then the boundary conditions since boundary conditions
   * are not set in the second operation on degrees of freedom that are
   * already constrained. This makes sure that the discretization remains
   * conforming as is needed. See the discussion on conflicting constraints in
   * the module on
   * @ref constraints.
   *
   * The parameter @p boundary_component corresponds to the number @p
   * boundary_id of the face.
   *
   * The flags in the last parameter, @p component_mask denote which
   * components of the finite element space shall be interpolated. If it is
   * left as specified by the default value (i.e. an empty array), all
   * components are interpolated. If it is different from the default value,
   * it is assumed that the number of entries equals the number of components
   * in the boundary functions and the finite element, and those components in
   * the given boundary function will be used for which the respective flag
   * was set in the component mask. See also
   * @ref GlossComponentMask.
   * As an example, assume that you are solving the Stokes equations in 2d,
   * with variables $(u,v,p)$ and that you only want to interpolate boundary
   * values for the pressure, then the component mask should correspond to
   * <code>(true,true,false)</code>.
   *
   * @note Whether a component mask has been specified or not, the number of
   * components of the functions in @p function_map must match that of the
   * finite element used by @p dof. In other words, for the example above, you
   * need to provide a Function object that has 3 components (the two
   * velocities and the pressure), even though you are only interested in the
   * first two of them. interpolate_boundary_values() will then call this
   * function to obtain a vector of 3 values at each interpolation point but
   * only take the first two and discard the third. In other words, you are
   * free to return whatever you like in the third component of the vector
   * returned by Function::vector_value, but the Function object must state
   * that it has 3 components.
   *
   * If the finite element used has shape functions that are non-zero in more
   * than one component (in deal.II speak: they are non-primitive), then these
   * components can presently not be used for interpolating boundary values.
   * Thus, the elements in the component mask corresponding to the components
   * of these non-primitive shape functions must be @p false.
   *
   * See the general documentation of this namespace for more information.
   *
   * @ingroup constraints
   */
  template <int dim,
            int spacedim,
            template <int, int> class DoFHandlerType,
            typename number>
  void
  interpolate_boundary_values(
    const Mapping<dim, spacedim> &       mapping,
    const DoFHandlerType<dim, spacedim> &dof,
    const std::map<types::boundary_id, const Function<spacedim, number> *>
      &                        function_map,
    AffineConstraints<number> &constraints,
    const ComponentMask &      component_mask = ComponentMask());

  /**
   * Same function as above, but taking only one pair of boundary indicator
   * and corresponding boundary function. The same comments apply as for the
   * previous function, in particular about the use of the component mask and
   * the requires size of the function object.
   *
   * @ingroup constraints
   *
   * @see
   * @ref GlossBoundaryIndicator "Glossary entry on boundary indicators"
   */
  template <int dim,
            int spacedim,
            template <int, int> class DoFHandlerType,
            typename number>
  void
  interpolate_boundary_values(
    const Mapping<dim, spacedim> &       mapping,
    const DoFHandlerType<dim, spacedim> &dof,
    const types::boundary_id             boundary_component,
    const Function<spacedim, number> &   boundary_function,
    AffineConstraints<number> &          constraints,
    const ComponentMask &                component_mask = ComponentMask());

  /**
   * Call the other interpolate_boundary_values() function, see above, with
   * <tt>mapping=MappingQGeneric@<dim,spacedim@>(1)</tt>. The same comments
   * apply as for the previous function, in particular about the use of the
   * component mask and the requires size of the function object.
   *
   * @ingroup constraints
   *
   * @see
   * @ref GlossBoundaryIndicator "Glossary entry on boundary indicators"
   */
  template <int dim,
            int spacedim,
            template <int, int> class DoFHandlerType,
            typename number>
  void
  interpolate_boundary_values(
    const DoFHandlerType<dim, spacedim> &dof,
    const types::boundary_id             boundary_component,
    const Function<spacedim, number> &   boundary_function,
    AffineConstraints<number> &          constraints,
    const ComponentMask &                component_mask = ComponentMask());


  /**
   * Call the other interpolate_boundary_values() function, see above, with
   * <tt>mapping=MappingQGeneric@<dim,spacedim@>(1)</tt>. The same comments
   * apply as for the previous function, in particular about the use of the
   * component mask and the requires size of the function object.
   *
   * @ingroup constraints
   */
  template <int dim,
            int spacedim,
            template <int, int> class DoFHandlerType,
            typename number>
  void
  interpolate_boundary_values(
    const DoFHandlerType<dim, spacedim> &dof,
    const std::map<types::boundary_id, const Function<spacedim, number> *>
      &                        function_map,
    AffineConstraints<number> &constraints,
    const ComponentMask &      component_mask = ComponentMask());


  /**
   * Project a function or a set of functions to the boundary of the domain.
   * In other words, compute the solution of the following problem: Find $u_h
   * \in V_h$ (where $V_h$ is the finite element space represented by the
   * DoFHandler argument of this function) so that
   * @f{align*}{
   * \int_{\Gamma} \varphi_i u_h
   * = \sum_{k \in {\cal K}} \int_{\Gamma_k} \varphi_i f_k,
   * \qquad \forall \varphi_i \in V_h
   * @f}
   * where $\Gamma = \bigcup_{k \in {\cal K}} \Gamma_k$, $\Gamma_k \subset
   * \partial\Omega$, $\cal K$ is the set of indices and $f_k$ the
   * corresponding boundary functions represented in the function map argument
   * @p boundary_values to this function, and the integrals are evaluated by
   * quadrature. This problem has a non-unique solution in the interior, but
   * it is well defined for the degrees of freedom on the part of the
   * boundary, $\Gamma$, for which we do the integration. The values of
   * $u_h|_\Gamma$, i.e., the nodal values of the degrees of freedom of this
   * function along the boundary, are then what is computed by this function.
   *
   * In case this function is used with $H_{div}$ conforming finite element
   * space, the solution of a different problem is computed, namely: Find
   * $\vec{u}_h \in V_h \subset H(\text{div}; \Omega)$ so that
   * @f{align*}{
   * \int_{\Gamma} (\vec{\varphi}_i \cdot \vec{n}) (\vec{u}_h \cdot \vec{n})
   * = \sum_{k \in {\cal K}} \int_{\Gamma_k} (\vec{\varphi}_i \cdot \vec{n})
   * (\vec{f}_k \cdot \vec{n}),
   * \qquad \forall \vec{\varphi_i} \in V_h,
   * @f}
   * where $\vec{n}$ is an outward normal vector.
   *
   * This function throws exception if used with $H_{curl}$ conforming elements,
   * so the project_boundary_values_curl_conforming() should be used instead.
   *
   * @param[in] mapping The mapping that will be used in the transformations
   * necessary to integrate along the boundary.
   * @param[in] dof The DoFHandler that describes the finite element space and
   * the numbering of degrees of freedom.
   * @param[in] boundary_functions A map from boundary indicators to pointers
   * to functions that describe the desired values on those parts of the
   * boundary marked with this boundary indicator (see
   * @ref GlossBoundaryIndicator "Boundary indicator").
   * The projection happens on only those parts of the boundary whose
   * indicators are represented in this map.
   * @param[in] q The face quadrature used in the integration necessary to
   * compute the mass matrix and right hand side of the projection.
   * @param[out] boundary_values The result of this function. It is a map
   * containing all indices of degrees of freedom at the boundary (as covered
   * by the boundary parts in @p boundary_functions) and the computed dof
   * value for this degree of freedom. For each degree of freedom at the
   * boundary, if its index already exists in @p boundary_values then its
   * boundary value will be overwritten, otherwise a new entry with proper
   * index and boundary value for this degree of freedom will be inserted into
   * @p boundary_values.
   * @param[in] component_mapping It is sometimes convenient to project a
   * vector-valued function onto only parts of a finite element space (for
   * example, to project a function with <code>dim</code> components onto the
   * velocity components of a <code>dim+1</code> component DoFHandler for a
   * Stokes problem). To allow for this, this argument allows components to be
   * remapped. If the vector is not empty, it has to have one entry for each
   * vector component of the finite element used in @p dof. This entry is the
   * component number in @p boundary_functions that should be used for this
   * component in @p dof. By default, no remapping is applied.
   */
  template <int dim, int spacedim, typename number>
  void
  project_boundary_values(
    const Mapping<dim, spacedim> &   mapping,
    const DoFHandler<dim, spacedim> &dof,
    const std::map<types::boundary_id, const Function<spacedim, number> *>
      &                                        boundary_functions,
    const Quadrature<dim - 1> &                q,
    std::map<types::global_dof_index, number> &boundary_values,
    std::vector<unsigned int>                  component_mapping = {});

  /**
   * Call the project_boundary_values() function, see above, with
   * <tt>mapping=MappingQGeneric@<dim,spacedim@>(1)</tt>.
   */
  template <int dim, int spacedim, typename number>
  void
  project_boundary_values(
    const DoFHandler<dim, spacedim> &dof,
    const std::map<types::boundary_id, const Function<spacedim, number> *>
      &                                        boundary_function,
    const Quadrature<dim - 1> &                q,
    std::map<types::global_dof_index, number> &boundary_values,
    std::vector<unsigned int>                  component_mapping = {});

  /**
   * Same as above, but for objects of type hp::DoFHandler
   */
  template <int dim, int spacedim, typename number>
  void
  project_boundary_values(
    const hp::MappingCollection<dim, spacedim> &mapping,
    const hp::DoFHandler<dim, spacedim> &       dof,
    const std::map<types::boundary_id, const Function<spacedim, number> *>
      &                                        boundary_functions,
    const hp::QCollection<dim - 1> &           q,
    std::map<types::global_dof_index, number> &boundary_values,
    std::vector<unsigned int>                  component_mapping = {});

  /**
   * Call the project_boundary_values() function, see above, with
   * <tt>mapping=MappingQGeneric@<dim,spacedim@>(1)</tt>.
   */
  template <int dim, int spacedim, typename number>
  void
  project_boundary_values(
    const hp::DoFHandler<dim, spacedim> &dof,
    const std::map<types::boundary_id, const Function<spacedim, number> *>
      &                                        boundary_function,
    const hp::QCollection<dim - 1> &           q,
    std::map<types::global_dof_index, number> &boundary_values,
    std::vector<unsigned int>                  component_mapping = {});

  /**
   * Project a function to the boundary of the domain, using the given
   * quadrature formula for the faces. This function identifies the degrees of
   * freedom subject to Dirichlet boundary conditions, adds them to the list
   * of constrained DoFs in @p constraints and sets the respective
   * inhomogeneity to the value resulting from the projection operation. If
   * this routine encounters a DoF that already is constrained (for instance
   * by a hanging node constraint, see below, or any other type of constraint,
   * e.g. from periodic boundary conditions), the old setting of the
   * constraint (dofs the entry is constrained to, inhomogeneities) is kept
   * and nothing happens.
   *
   * @note When combining adaptively refined meshes with hanging node
   * constraints and boundary conditions like from the current function within
   * one AffineConstraints object, the hanging node constraints should always
   * be set first, and then the boundary conditions since boundary conditions
   * are not set in the second operation on degrees of freedom that are
   * already constrained. This makes sure that the discretization remains
   * conforming as is needed. See the discussion on conflicting constraints in
   * the module on
   * @ref constraints.
   *
   * If @p component_mapping is empty, it is assumed that the number of
   * components of @p boundary_function matches that of the finite element
   * used by @p dof.
   *
   * In 1d, projection equals interpolation. Therefore,
   * interpolate_boundary_values is called.
   *
   * @arg @p component_mapping: if the components in @p boundary_functions and
   * @p dof do not coincide, this vector allows them to be remapped. If the
   * vector is not empty, it has to have one entry for each component in @p
   * dof. This entry is the component number in @p boundary_functions that
   * should be used for this component in @p dof. By default, no remapping is
   * applied.
   *
   * @ingroup constraints
   */
  template <int dim, int spacedim, typename number>
  void
  project_boundary_values(
    const Mapping<dim, spacedim> &   mapping,
    const DoFHandler<dim, spacedim> &dof,
    const std::map<types::boundary_id, const Function<spacedim, number> *>
      &                        boundary_functions,
    const Quadrature<dim - 1> &q,
    AffineConstraints<number> &constraints,
    std::vector<unsigned int>  component_mapping = {});

  /**
   * Call the project_boundary_values() function, see above, with
   * <tt>mapping=MappingQGeneric@<dim,spacedim@>(1)</tt>.
   *
   * @ingroup constraints
   */
  template <int dim, int spacedim, typename number>
  void
  project_boundary_values(
    const DoFHandler<dim, spacedim> &dof,
    const std::map<types::boundary_id, const Function<spacedim, number> *>
      &                        boundary_function,
    const Quadrature<dim - 1> &q,
    AffineConstraints<number> &constraints,
    std::vector<unsigned int>  component_mapping = {});


  /**
   * Compute constraints that correspond to boundary conditions of the form
   * $\vec{n}\times\vec{u}=\vec{n}\times\vec{f}$, i.e. the tangential
   * components of $u$ and $f$ shall coincide.
   *
   * If the AffineConstraints @p constraints contained values or other
   * constraints before, the new ones are added or the old ones overwritten,
   * if a node of the boundary part to be used was already in the list of
   * constraints. This is handled by using inhomogeneous constraints. Please
   * note that when combining adaptive meshes and this kind of constraints,
   * the Dirichlet conditions should be set first, and then completed by
   * hanging node constraints, in order to make sure that the discretization
   * remains consistent. See the discussion on conflicting constraints in the
   * module on
   * @ref constraints.
   *
   * This function is explicitly written to use with the FE_Nedelec elements.
   * Thus it throws an exception, if it is called with other finite elements.
   *
   * The second argument of this function denotes the first vector component
   * in the finite element that corresponds to the vector function that you
   * want to constrain. For example, if we want to solve Maxwell's equations
   * in 3d and the finite element has components $(E_x,E_y,E_z,B_x,B_y,B_z)$
   * and we want the boundary conditions
   * $\vec{n}\times\vec{B}=\vec{n}\times\vec{f}$, then @p
   * first_vector_component would be 3. Vectors are implicitly assumed to have
   * exactly <code>dim</code> components that are ordered in the same way as
   * we usually order the coordinate directions, i.e. $x$-, $y$-, and finally
   * $z$-component.
   *
   * The parameter @p boundary_component corresponds to the number @p
   * boundary_id of the face. numbers::internal_face_boundary_id is an illegal
   * value, since it is reserved for interior faces.
   *
   * The last argument is denoted to compute the normal vector $\vec{n}$ at
   * the boundary points.
   *
   * <h4>Computing constraints</h4>
   *
   * To compute the constraints we use projection-based interpolation as
   * proposed in Solin, Segeth and Dolezel (Higher order finite elements,
   * Chapman&amp;Hall, 2004) on every face located at the boundary.
   *
   * First one projects $\vec{f}$ on the lowest-order edge shape functions.
   * Then the remaining part $(I-P_0)\vec{f}$ of the function is projected on
   * the remaining higher-order edge shape functions. In the last step we
   * project $(I-P_0-P_e)\vec{f}$ on the bubble shape functions defined on the
   * face.
   *
   * @deprecated Use the project_boundary_values_curl_conforming_l2() function
   * instead of this one.
   *
   * @ingroup constraints
   *
   * @see
   * @ref GlossBoundaryIndicator "Glossary entry on boundary indicators"
   */
  template <int dim>
  DEAL_II_DEPRECATED void
  project_boundary_values_curl_conforming(
    const DoFHandler<dim> &      dof_handler,
    const unsigned int           first_vector_component,
    const Function<dim, double> &boundary_function,
    const types::boundary_id     boundary_component,
    AffineConstraints<double> &  constraints,
    const Mapping<dim> &         mapping = StaticMappingQ1<dim>::mapping);

  /**
   * Same as above for the hp-namespace.
   *
   * @deprecated Use the project_boundary_values_curl_conforming_l2() function
   * instead of this one.
   *
   * @ingroup constraints
   *
   * @see
   * @ref GlossBoundaryIndicator "Glossary entry on boundary indicators"
   */
  template <int dim>
  DEAL_II_DEPRECATED void
  project_boundary_values_curl_conforming(
    const hp::DoFHandler<dim> &            dof_handler,
    const unsigned int                     first_vector_component,
    const Function<dim, double> &          boundary_function,
    const types::boundary_id               boundary_component,
    AffineConstraints<double> &            constraints,
    const hp::MappingCollection<dim, dim> &mapping_collection =
      hp::StaticMappingQ1<dim>::mapping_collection);

  /**
   * This function is an updated version of the
   * project_boundary_values_curl_conforming function. The intention is to fix
   * a problem when using the previous function in conjunction with non-
   * rectangular geometries (i.e. elements with non-rectangular faces). The
   * L2-projection method used has been taken from the paper "Electromagnetic
   * scattering simulation using an H (curl) conforming hp finite element
   * method in three dimensions" by PD Ledger, K Morgan and O Hassan ( Int. J.
   * Num. Meth. Fluids, Volume 53, Issue 8, pages 1267–1296).
   *
   * This function will compute constraints that correspond to Dirichlet
   * boundary conditions of the form
   * $\vec{n}\times\vec{E}=\vec{n}\times\vec{F}$ i.e. the tangential
   * components of $\vec{E}$ and $f$ shall coincide.
   *
   * <h4>Computing constraints</h4>
   *
   * To compute the constraints we use a projection method based upon the
   * paper mentioned above. In 2D this is done in a single stage for the edge-
   * based shape functions, regardless of the order of the finite element. In
   * 3D this is done in two stages, edges first and then faces.
   *
   * For each cell, each edge, $e$, is projected by solving the linear system
   * $Ax=b$ where $x$ is the vector of constraints on degrees of freedom on the
   * edge and
   *
   * $A_{ij} = \int_{e} (\vec{s}_{i}\cdot\vec{t})(\vec{s}_{j}\cdot\vec{t}) dS$
   *
   * $b_{i} = \int_{e} (\vec{s}_{i}\cdot\vec{t})(\vec{F}\cdot\vec{t}) dS$
   *
   * with $\vec{s}_{i}$ the $i^{th}$ shape function and $\vec{t}$ the tangent
   * vector.
   *
   * Once all edge constraints, $x$, have been computed, we may compute the
   * face constraints in a similar fashion, taking into account the residuals
   * from the edges.
   *
   * For each face on the cell, $f$, we solve the linear system $By=c$ where
   * $y$ is the vector of constraints on degrees of freedom on the face and
   *
   * $B_{ij} = \int_{f} (\vec{n} \times \vec{s}_{i}) \cdot (\vec{n} \times
   * \vec{s}_{j}) dS$
   *
   * $c_{i} = \int_{f} (\vec{n} \times \vec{r}) \cdot (\vec{n} \times
   * \vec{s}_i) dS$
   *
   * and $\vec{r} = \vec{F} - \sum_{e \in f} \sum{i \in e} x_{i}\vec{s}_i$,
   * the edge residual.
   *
   * The resulting constraints are then given in the solutions $x$ and $y$.
   *
   * If the AffineConstraints @p constraints contained values or other
   * constraints before, the new ones are added or the old ones overwritten,
   * if a node of the boundary part to be used was already in the list of
   * constraints. This is handled by using inhomogeneous constraints. Please
   * note that when combining adaptive meshes and this kind of constraints,
   * the Dirichlet conditions should be set first, and then completed by
   * hanging node constraints, in order to make sure that the discretization
   * remains consistent. See the discussion on conflicting constraints in the
   * module on
   * @ref constraints.
   *
   * <h4>Arguments to this function</h4>
   *
   * This function is explicitly for use with FE_Nedelec elements, or with
   * FESystem elements which contain FE_Nedelec elements. It will throw an
   * exception if called with any other finite element. The user must ensure
   * that FESystem elements are correctly setup when using this function as
   * this check not possible in this case.
   *
   * The second argument of this function denotes the first vector component
   * of the finite element which corresponds to the vector function that you
   * wish to constrain. For example, if we are solving Maxwell's equations in
   * 3D and have components $(E_x,E_y,E_z,B_x,B_y,B_z)$ and we want the
   * boundary conditions $\vec{n}\times\vec{B}=\vec{n}\times\vec{f}$, then @p
   * first_vector_component would be 3. The @p boundary_function must return 6
   * components in this example, with the first 3 corresponding to $\vec{E}$
   * and the second 3 corresponding to $\vec{B}$. Vectors are implicitly
   * assumed to have exactly <code>dim</code> components that are ordered in
   * the same way as we usually order the coordinate directions, i.e. $x$-,
   * $y$-, and finally $z$-component.
   *
   * The parameter @p boundary_component corresponds to the number @p
   * boundary_id of the face. numbers::internal_face_boundary_id is an illegal
   * value, since it is reserved for interior faces.
   *
   * The last argument is denoted to compute the normal vector $\vec{n}$ at
   * the boundary points.
   *
   *
   * @ingroup constraints
   *
   * @see
   * @ref GlossBoundaryIndicator "Glossary entry on boundary indicators"
   */
  template <int dim, typename number>
  void
  project_boundary_values_curl_conforming_l2(
    const DoFHandler<dim> &      dof_handler,
    const unsigned int           first_vector_component,
    const Function<dim, number> &boundary_function,
    const types::boundary_id     boundary_component,
    AffineConstraints<number> &  constraints,
    const Mapping<dim> &         mapping = StaticMappingQ1<dim>::mapping);


  /**
   * hp-namespace version of project_boundary_values_curl_conforming_l2
   * (above).
   *
   * @ingroup constraints
   */
  template <int dim, typename number>
  void
  project_boundary_values_curl_conforming_l2(
    const hp::DoFHandler<dim> &            dof_handler,
    const unsigned int                     first_vector_component,
    const Function<dim, number> &          boundary_function,
    const types::boundary_id               boundary_component,
    AffineConstraints<number> &            constraints,
    const hp::MappingCollection<dim, dim> &mapping_collection =
      hp::StaticMappingQ1<dim>::mapping_collection);


  /**
   * Compute constraints that correspond to boundary conditions of the form
   * $\vec{n}^T\vec{u}=\vec{n}^T\vec{f}$, i.e. the normal components of the
   * solution $u$ and a given $f$ shall coincide. The function $f$ is given by
   * @p boundary_function and the resulting constraints are added to @p
   * constraints for faces with boundary indicator @p boundary_component.
   *
   * This function is explicitly written to use with the FE_RaviartThomas
   * elements. Thus it throws an exception, if it is called with other finite
   * elements.
   *
   * If the AffineConstraints object @p constraints contained values or other
   * constraints before, the new ones are added or the old ones overwritten,
   * if a node of the boundary part to be used was already in the list of
   * constraints. This is handled by using inhomogeneous constraints. Please
   * note that when combining adaptive meshes and this kind of constraints,
   * the Dirichlet conditions should be set first, and then completed by
   * hanging node constraints, in order to make sure that the discretization
   * remains consistent. See the discussion on conflicting constraints in the
   * module on
   * @ref constraints.
   *
   * The argument @p first_vector_component denotes the first vector component
   * in the finite element that corresponds to the vector function $\vec{u}$
   * that you want to constrain. Vectors are implicitly assumed to have
   * exactly <code>dim</code> components that are ordered in the same way as
   * we usually order the coordinate directions, i.e., $x$-, $y$-, and finally
   * $z$-component.
   *
   * The parameter @p boundary_component corresponds to the @p boundary_id of
   * the faces where the boundary conditions are applied.
   * numbers::internal_face_boundary_id is an illegal value, since it is
   * reserved for interior faces. The @p mapping is used to compute the normal
   * vector $\vec{n}$ at the boundary points.
   *
   * <h4>Computing constraints</h4>
   *
   * To compute the constraints we use interpolation operator proposed in
   * Brezzi, Fortin (Mixed and Hybrid (Finite Element Methods, Springer, 1991)
   * on every face located at the boundary.
   *
   * @ingroup constraints
   *
   * @see
   * @ref GlossBoundaryIndicator "Glossary entry on boundary indicators"
   */
  template <int dim>
  void
  project_boundary_values_div_conforming(
    const DoFHandler<dim> &      dof_handler,
    const unsigned int           first_vector_component,
    const Function<dim, double> &boundary_function,
    const types::boundary_id     boundary_component,
    AffineConstraints<double> &  constraints,
    const Mapping<dim> &         mapping = StaticMappingQ1<dim>::mapping);

  /**
   * Same as above for the hp-namespace.
   *
   * @ingroup constraints
   *
   * @see
   * @ref GlossBoundaryIndicator "Glossary entry on boundary indicators"
   */
  template <int dim>
  void
  project_boundary_values_div_conforming(
    const hp::DoFHandler<dim> &            dof_handler,
    const unsigned int                     first_vector_component,
    const Function<dim, double> &          boundary_function,
    const types::boundary_id               boundary_component,
    AffineConstraints<double> &            constraints,
    const hp::MappingCollection<dim, dim> &mapping_collection =
      hp::StaticMappingQ1<dim>::mapping_collection);


  /**
   * This function computes the constraints that correspond to boundary
   * conditions of the form $\vec u \cdot \vec n=\vec u_\Gamma \cdot \vec n$,
   * i.e., normal flux constraints where $\vec u$ is a vector-valued solution
   * variable and $\vec u_\Gamma$ is a prescribed vector field whose normal
   * component we want to be equal to the normal component of the solution.
   * These conditions have exactly the form handled by the
   * AffineConstraints class, in that they relate a <i>linear
   * combination</i> of boundary degrees of freedom to a corresponding
   * value (the inhomogeneity of the constraint). Consequently, the current
   * function creates a list of constraints that are written into an
   * AffineConstraints container. This object may already have some
   * content, for example from hanging node constraints, that remains
   * untouched. These constraints have to be applied to the linear system
   * like any other such constraints, i.e., you have to condense the linear
   * system with the constraints before solving, and you have to distribute
   * the solution vector afterwards.
   *
   * This function treats a more general case than
   * VectorTools::compute_no_normal_flux_constraints() (which can only handle
   * the case where $\vec u_\Gamma \cdot \vec n = 0$, and is used in
   * step-31 and step-32). However, because everything that would apply
   * to that function also applies as a special case to the current
   * function, the following discussion is relevant to both.
   *
   * @note This function doesn't make much sense in 1d, so it throws an
   *   exception if @p dim equals one.
   *
   *
   * <h4>Arguments to this function</h4>
   *
   * The second argument of this function denotes the first vector component
   * in the finite element that corresponds to the vector function that you
   * want to constrain. For example, if we were solving a Stokes equation in
   * 2d and the finite element had components $(u,v,p)$, then @p
   * first_vector_component needs to be zero if you intend to constraint
   * the vector $(u,v)^T \cdot \vec n = \vec u_\Gamma \cdot \vec n$.
   * On the other hand, if we solved the
   * Maxwell equations in 3d and the finite element has components
   * $(E_x,E_y,E_z,B_x,B_y,B_z)$ and we want the boundary condition $\vec
   * B\cdot \vec n=\vec B_\Gamma\cdot \vec n$, then @p first_vector_component
   * would be 3. Vectors are implicitly assumed to have exactly
   * <code>dim</code> components that are ordered in the same way as we
   * usually order the coordinate directions, i.e. $x$-, $y$-, and finally
   * $z$-component. The function assumes, but can't check, that the vector
   * components in the range
   * <code>[first_vector_component,first_vector_component+dim)</code> come
   * from the same base finite element. For example, in the Stokes example
   * above, it would not make sense to use a
   * <code>FESystem@<dim@>(FE_Q@<dim@>(2), 1, FE_Q@<dim@>(1), dim)</code>
   * (note that the first velocity vector component is a $Q_2$ element,
   * whereas all the other ones are $Q_1$ elements) as there would be points
   * on the boundary where the $x$-velocity is defined but no corresponding
   * $y$- or $z$-velocities.
   *
   * The third argument denotes the set of boundary indicators on which the
   * boundary condition is to be enforced. Note that, as explained below, this
   * is one of the few functions where it makes a difference where we call the
   * function multiple times with only one boundary indicator, or whether we
   * call the function once with the whole set of boundary indicators at once.
   *
   * Argument four (@p function_map) describes the boundary function $\vec
   * u_\Gamma$ for each boundary id. The function <code>function_map[id]</code>
   * is used on boundary with id @p id taken from the set @p boundary_ids.
   * Each function in @p function_map is expected to have @p dim
   * components, which are used independent of @p first_vector_component.
   *
   * The mapping argument is used to compute the boundary points at which the
   * function needs to request the normal vector $\vec n$ from the boundary
   * description.
   *
   * @note When combining adaptively refined meshes with hanging node
   * constraints and boundary conditions like from the current function within
   * one AffineConstraints object, the hanging node constraints should always
   * be set first, and then the boundary conditions since boundary conditions
   * are not set in the second operation on degrees of freedom that are
   * already constrained. This makes sure that the discretization remains
   * conforming as is needed. See the discussion on conflicting constraints in
   * the module on
   * @ref constraints.
   *
   *
   * <h4>Computing constraints in 2d</h4>
   *
   * Computing these constraints requires some smarts. The main question
   * revolves around the question what the normal vector is. Consider the
   * following situation:
   *
   * <p ALIGN="center">
   * @image html no_normal_flux_1.png
   * </p>
   *
   * Here, we have two cells that use a bilinear mapping (i.e.,
   * MappingQGeneric(1)). Consequently, for each of the cells, the normal
   * vector is perpendicular to the straight edge. If the two edges at the top
   * and right are meant to approximate a curved boundary (as indicated by the
   * dashed line), then neither of the two computed normal vectors are equal
   * to the exact normal vector (though they approximate it as the mesh is
   * refined further). What is worse, if we constrain $\vec u \cdot \vec n=
   * \vec u_\Gamma \cdot \vec n$ at the common vertex with the normal vector
   * from both cells, then we constrain the vector $\vec u$ with respect to
   * two linearly independent vectors; consequently, the constraint would be
   * $\vec u=\vec u_\Gamma$ at this point (i.e. <i>all</i> components of the
   * vector), which is not what we wanted.
   *
   * To deal with this situation, the algorithm works in the following way: at
   * each point where we want to constrain $\vec u$, we first collect all
   * normal vectors that adjacent cells might compute at this point. We then
   * do not constrain $\vec u \cdot \vec n=\vec u_\Gamma \cdot \vec n$ for
   * <i>each</i> of these normal vectors but only for the <i>average</i> of
   * the normal vectors. In the example above, we therefore record only a
   * single constraint $\vec u \cdot \vec {\bar n}=\vec u_\Gamma \cdot \vec
   * {\bar n}$, where $\vec {\bar n}$ is the average of the two indicated
   * normal vectors.
   *
   * Unfortunately, this is not quite enough. Consider the situation here:
   *
   * <p ALIGN="center">
   * @image html no_normal_flux_2.png
   * </p>
   *
   * If again the top and right edges approximate a curved boundary, and the
   * left boundary a separate boundary (for example straight) so that the
   * exact boundary has indeed a corner at the top left vertex, then the above
   * construction would not work: here, we indeed want the constraint that
   * $\vec u$ at this point (because the normal velocities with respect to
   * both the left normal as well as the top normal vector should be zero),
   * not that the velocity in the direction of the average normal vector is
   * zero.
   *
   * Consequently, we use the following heuristic to determine whether all
   * normal vectors computed at one point are to be averaged: if two normal
   * vectors for the same point are computed on <i>different</i> cells, then
   * they are to be averaged. This covers the first example above. If they are
   * computed from the same cell, then the fact that they are different is
   * considered indication that they come from different parts of the boundary
   * that might be joined by a real corner, and must not be averaged.
   *
   * There is one problem with this scheme. If, for example, the same domain
   * we have considered above, is discretized with the following mesh, then we
   * get into trouble:
   *
   * <p ALIGN="center">
   * @image html no_normal_flux_3.png
   * </p>
   *
   * Here, the algorithm assumes that the boundary does not have a corner at
   * the point where faces $F1$ and $F2$ join because at that point there are
   * two different normal vectors computed from different cells. If you intend
   * for there to be a corner of the exact boundary at this point, the only
   * way to deal with this is to assign the two parts of the boundary
   * different boundary indicators and call this function twice, once for each
   * boundary indicators; doing so will yield only one normal vector at this
   * point per invocation (because we consider only one boundary part at a
   * time), with the result that the normal vectors will not be averaged. This
   * situation also needs to be taken into account when using this function
   * around reentrant corners on Cartesian meshes. If normal-flux boundary
   * conditions are to be enforced on non-Cartesian meshes around reentrant
   * corners, one may even get cycles in the constraints as one will in
   * general constrain different components from the two sides. In that case,
   * set a no-slip constraint on the reentrant vertex first.
   *
   *
   * <h4>Computing constraints in 3d</h4>
   *
   * The situation is more complicated in 3d. Consider the following case
   * where we want to compute the constraints at the marked vertex:
   *
   * <p ALIGN="center">
   * @image html no_normal_flux_4.png
   * </p>
   *
   * Here, we get four different normal vectors, one from each of the four
   * faces that meet at the vertex. Even though they may form a complete set
   * of vectors, it is not our intent to constrain all components of the
   * vector field at this point. Rather, we would like to still allow
   * tangential flow, where the term "tangential" has to be suitably defined.
   *
   * In a case like this, the algorithm proceeds as follows: for each cell
   * that has computed two tangential vectors at this point, we compute the
   * unconstrained direction as the outer product of the two tangential
   * vectors (if necessary multiplied by minus one). We then average these
   * tangential vectors. Finally, we compute constraints for the two
   * directions perpendicular to this averaged tangential direction.
   *
   * There are cases where one cell contributes two tangential directions and
   * another one only one; for example, this would happen if both top and
   * front faces of the left cell belong to the boundary selected whereas only
   * the top face of the right cell belongs to it, maybe indicating that the
   * entire front part of the domain is a smooth manifold whereas the top
   * really forms two separate manifolds that meet in a ridge, and that
   * normal-flux boundary conditions are only desired on the front manifold
   * and the right one on top. In cases like these, it's difficult to define
   * what should happen. The current implementation simply ignores the one
   * contribution from the cell that only contributes one normal vector. In
   * the example shown, this is acceptable because the normal vector for the
   * front face of the left cell is the same as the normal vector provided by
   * the front face of the right cell (the surface is planar) but it would be
   * a problem if the front manifold would be curved. Regardless, it is
   * unclear how one would proceed in this case and ignoring the single cell
   * is likely the best one can do.
   *
   *
   * <h4>Results</h4>
   *
   * Because it makes for good pictures, here are two images of vector fields
   * on a circle and on a sphere to which the constraints computed by this
   * function have been applied (for illustration purposes, we enforce zero
   * normal flux, which can more easily be computed using
   * VectorTools::compute_no_normal_flux_constraints(), as this must
   * lead to a <i>tangential</i> vector field):
   *
   * <p ALIGN="center">
   * @image html no_normal_flux_5.png
   * @image html no_normal_flux_6.png
   * </p>
   *
   * The vectors fields are not physically reasonable but the tangentiality
   * constraint is clearly enforced. The fact that the vector fields are zero
   * at some points on the boundary is an artifact of the way it is created,
   * it is not constrained to be zero at these points.
   *
   * @ingroup constraints
   *
   * @see
   * @ref GlossBoundaryIndicator "Glossary entry on boundary indicators"
   */
  template <int dim, int spacedim, template <int, int> class DoFHandlerType>
  void
  compute_nonzero_normal_flux_constraints(
    const DoFHandlerType<dim, spacedim> &dof_handler,
    const unsigned int                   first_vector_component,
    const std::set<types::boundary_id> & boundary_ids,
    const std::map<types::boundary_id, const Function<spacedim> *>
      &                           function_map,
    AffineConstraints<double> &   constraints,
    const Mapping<dim, spacedim> &mapping = StaticMappingQ1<dim>::mapping);

  /**
   * This function does the same as the
   * compute_nonzero_normal_flux_constraints() function (see there for more
   * information), but for the simpler case of homogeneous normal-flux
   * constraints, i.e., for imposing the condition
   * $\vec u \cdot \vec n= 0$. This function is used in step-31 and step-32.
   *
   * @ingroup constraints
   *
   * @see
   * @ref GlossBoundaryIndicator "Glossary entry on boundary indicators"
   */
  template <int dim, int spacedim, template <int, int> class DoFHandlerType>
  void
  compute_no_normal_flux_constraints(
    const DoFHandlerType<dim, spacedim> &dof_handler,
    const unsigned int                   first_vector_component,
    const std::set<types::boundary_id> & boundary_ids,
    AffineConstraints<double> &          constraints,
    const Mapping<dim, spacedim> &mapping = StaticMappingQ1<dim>::mapping);

  /**
   * Compute the constraints that correspond to boundary conditions of the
   * form $\vec u \times \vec n=\vec u_\Gamma \times \vec n$, i.e., tangential
   * flow constraints where $\vec u$ is a vector-valued solution
   * variable and $\vec u_\Gamma$ is prescribed vector field whose tangential
   * component(s) we want to be equal to the tangential component(s) of the
   * solution. This function constrains exactly those dim-1 vector-valued
   * components that are left unconstrained by
   * VectorTools::compute_no_normal_flux_constraints(), and leaves the one
   * component unconstrained that is constrained by that function.
   *
   * @ingroup constraints
   *
   * @see
   * @ref GlossBoundaryIndicator "Glossary entry on boundary indicators"
   */
  template <int dim, int spacedim, template <int, int> class DoFHandlerType>
  void
  compute_nonzero_tangential_flux_constraints(
    const DoFHandlerType<dim, spacedim> &dof_handler,
    const unsigned int                   first_vector_component,
    const std::set<types::boundary_id> & boundary_ids,
    const std::map<types::boundary_id, const Function<spacedim> *>
      &                           function_map,
    AffineConstraints<double> &   constraints,
    const Mapping<dim, spacedim> &mapping = StaticMappingQ1<dim>::mapping);

  /**
   * Same as above for homogeneous tangential-flux constraints.
   *
   * @ingroup constraints
   *
   * @see
   * @ref GlossBoundaryIndicator "Glossary entry on boundary indicators"
   */
  template <int dim, int spacedim, template <int, int> class DoFHandlerType>
  void
  compute_normal_flux_constraints(
    const DoFHandlerType<dim, spacedim> &dof_handler,
    const unsigned int                   first_vector_component,
    const std::set<types::boundary_id> & boundary_ids,
    AffineConstraints<double> &          constraints,
    const Mapping<dim, spacedim> &mapping = StaticMappingQ1<dim>::mapping);


  //@}
  /**
   * @name Assembling of right hand sides
   */
  //@{

  /**
   * Create a right hand side vector. Prior content of the given @p rhs_vector
   * vector is deleted.
   *
   * See the general documentation of this namespace for further information.
   */
  template <int dim, int spacedim, typename VectorType>
  void
  create_right_hand_side(
    const Mapping<dim, spacedim> &                             mapping,
    const DoFHandler<dim, spacedim> &                          dof,
    const Quadrature<dim> &                                    q,
    const Function<spacedim, typename VectorType::value_type> &rhs,
    VectorType &                                               rhs_vector,
    const AffineConstraints<typename VectorType::value_type> & constraints =
      AffineConstraints<typename VectorType::value_type>());

  /**
   * Call the create_right_hand_side() function, see above, with
   * <tt>mapping=MappingQGeneric@<dim@>(1)</tt>.
   */
  template <int dim, int spacedim, typename VectorType>
  void
  create_right_hand_side(
    const DoFHandler<dim, spacedim> &                          dof,
    const Quadrature<dim> &                                    q,
    const Function<spacedim, typename VectorType::value_type> &rhs,
    VectorType &                                               rhs_vector,
    const AffineConstraints<typename VectorType::value_type> & constraints =
      AffineConstraints<typename VectorType::value_type>());

  /**
   * Like the previous set of functions, but for hp objects.
   */
  template <int dim, int spacedim, typename VectorType>
  void
  create_right_hand_side(
    const hp::MappingCollection<dim, spacedim> &               mapping,
    const hp::DoFHandler<dim, spacedim> &                      dof,
    const hp::QCollection<dim> &                               q,
    const Function<spacedim, typename VectorType::value_type> &rhs,
    VectorType &                                               rhs_vector,
    const AffineConstraints<typename VectorType::value_type> & constraints =
      AffineConstraints<typename VectorType::value_type>());

  /**
   * Like the previous set of functions, but for hp objects.
   */
  template <int dim, int spacedim, typename VectorType>
  void
  create_right_hand_side(
    const hp::DoFHandler<dim, spacedim> &                      dof,
    const hp::QCollection<dim> &                               q,
    const Function<spacedim, typename VectorType::value_type> &rhs,
    VectorType &                                               rhs_vector,
    const AffineConstraints<typename VectorType::value_type> & constraints =
      AffineConstraints<typename VectorType::value_type>());

  /**
   * Create a right hand side vector for a point source at point @p p. In
   * other words, it creates a vector $F$ so that $F_i = \int_\Omega
   * \delta(x-p) \varphi_i(x) dx$ where $\varphi_i$ are the shape functions
   * described by @p dof_handler and @p p is the point at which the delta
   * function is located. Prior content of the given @p rhs_vector
   * vector is deleted. This function is for the case of a scalar finite
   * element.
   *
   * This function is typically used in one of these two contexts:
   * - Let's say you want to solve the same kind of problems many times
   *   over, with different values for right hand sides or coefficients,
   *   and then evaluate the solution at the same point every time. You
   *   could do this by calling VectorTools::point_value() after each
   *   solve, or you could realize that to evaluate the solution $u_h$
   *   at a point $p$, you could rearrange operations like this:
   *   @f{align*}{
   *     u_h(p) &= \sum_j U_j \varphi_j(p) = \sum_j U_j F_j
   *       \\   &= U \cdot F
   *   @f}
   *   with the vector as defined above. In other words, point evaluation
   *   can be achieved with just a single vector-vector product, and the
   *   vector $F$ can be computed once and for all and reused
   *   for each solve, without having to go through the mesh every time
   *   to find out which cell (and where in the cell) the point $p$ is
   *   located.
   * - This function is also useful if you wanted to compute the Green's
   *   function for the problem you are solving. This is because the
   *   Green's function $G(x,p)$ is defined by
   *   @f{align*}{
   *     L G(x,p) &= \delta(x-p)
   *   @f}
   *   where $L$ is the differential operator of your problem. The discrete
   *   version then requires computing the right hand side vector
   *   $F_i = \int_\Omega \varphi_i(x) \delta(x-p)$, which is exactly
   *   the vector computed by the current function.
   *
   * While maybe not relevant for documenting <i>what</i> this
   * function does, it may be interesting to note that delta functions
   * do not exist in reality, and consequently, using this function
   * does not model any real situation. This is, because no real
   * object is able to focus an infinite force density at an
   * infinitesimally small part of the domain (rather, all real
   * devices will spread out the force over a finite area); nor is it
   * possible to measure values at individual points (but all
   * measurements will somehow be averaged over small areas). Only if
   * this area is so small that it cannot be resolved by any mesh does
   * it make sense to model the situation in a way that uses a delta
   * function with the same overall force or sensitivity. On the other
   * hand, a situation that is probably more fruitfully simulated with
   * a delta function is the electric potential of a point source; in
   * this case, the solution is known to have a logarithmic
   * singularity (in 2d) or a $\frac{1}{r}$ singularity (in 3d),
   * neither of which is bounded.
   *
   * Mathematically, the use of delta functions typically leads to exact
   * solutions to which the numerically obtained, approximate solution does
   * not converge. This is because, taking the Laplace equation as an example,
   * the error between exact and numerical solution can be bounded by the
   * expression
   * @f{align*}{
   *   \| u-u_h \|_{L_2} \le C h \| \nabla u \|_{L_2}
   * @f}
   * but when using a delta function on the right hand side, the term
   * $\| \nabla u \|_{L_2} = |u|_{H^1}$ is not finite. This can be seen
   * by using the a-priori bound for solutions of the Laplace equation
   * $-\Delta u = f$ that states that $|u|_{H^1} \le \|f\|_{H^{-1}}$.
   * When using a delta function as right hand side, $f(x)=\delta(x-p)$,
   * one would need to take the $H^{-1}$ norm of a delta function, which
   * however is not finite because $\delta(\cdot-p) \not\in H^{-1}$.
   *
   * The consequence of all of this is that the exact solution of the
   * Laplace equation with a delta function on the right hand side --
   * i.e., the <i>Green's function</i> -- has a singularity at $p$ that
   * is so strong that it cannot be resolved by a finite element
   * solution, and consequently finite element approximations do not
   * converge towards the exact solution in any of the usual norms.
   *
   * All of this is also the case for all of the other usual second-order
   * partial differential equations in dimensions two or higher. (Because
   * in dimension two and higher, $H^1$ functions are not necessarily
   * continuous, and consequently the delta function is not in the dual
   * space $H^{-1}$.)
   */
  template <int dim, int spacedim>
  void
  create_point_source_vector(const Mapping<dim, spacedim> &   mapping,
                             const DoFHandler<dim, spacedim> &dof_handler,
                             const Point<spacedim> &          p,
                             Vector<double> &                 rhs_vector);

  /**
   * Call the create_point_source_vector() function, see above, with
   * an implied default $Q_1$ mapping object.
   */
  template <int dim, int spacedim>
  void
  create_point_source_vector(const DoFHandler<dim, spacedim> &dof_handler,
                             const Point<spacedim> &          p,
                             Vector<double> &                 rhs_vector);

  /**
   * Like the previous set of functions, but for hp objects.
   */
  template <int dim, int spacedim>
  void
  create_point_source_vector(
    const hp::MappingCollection<dim, spacedim> &mapping,
    const hp::DoFHandler<dim, spacedim> &       dof_handler,
    const Point<spacedim> &                     p,
    Vector<double> &                            rhs_vector);

  /**
   * Like the previous set of functions, but for hp objects. The function uses
   * an implied default $Q_1$ mapping object. Note that if your hp::DoFHandler
   * uses any active fe index other than zero, then you need to call the
   * function above that provides a mapping object for each active fe index.
   */
  template <int dim, int spacedim>
  void
  create_point_source_vector(const hp::DoFHandler<dim, spacedim> &dof_handler,
                             const Point<spacedim> &              p,
                             Vector<double> &                     rhs_vector);

  /**
   * Create a right hand side vector for a point source at point @p p. This
   * variation of the function is meant for vector-valued problems with
   * exactly dim components (it will also work for problems with more than dim
   * components, and in this case simply consider only the first dim
   * components of the shape functions). It computes a right hand side that
   * corresponds to a forcing function that is equal to a delta function times
   * a given direction. In other words, it creates a vector $F$ so that $F_i =
   * \int_\Omega [\mathbf d \delta(x-p)] \cdot \varphi_i(x) dx$. Note here that
   * $\varphi_i$ is a vector-valued function. $\mathbf d$ is the given direction
   * of the source term $\mathbf d \delta(x-p)$ and corresponds to the @p
   * direction argument to be passed to this function.
   *
   * Prior content of the given @p rhs_vector vector is deleted.
   *
   * See the discussion of the first create_point_source_vector() variant for
   * more on the use of delta functions.
   */
  template <int dim, int spacedim>
  void
  create_point_source_vector(const Mapping<dim, spacedim> &   mapping,
                             const DoFHandler<dim, spacedim> &dof_handler,
                             const Point<spacedim> &          p,
                             const Point<dim> &               direction,
                             Vector<double> &                 rhs_vector);

  /**
   * Call the create_point_source_vector() function for vector-valued finite
   * elements, see above, with an implied default $Q_1$ mapping object.
   */
  template <int dim, int spacedim>
  void
  create_point_source_vector(const DoFHandler<dim, spacedim> &dof_handler,
                             const Point<spacedim> &          p,
                             const Point<dim> &               direction,
                             Vector<double> &                 rhs_vector);

  /**
   * Like the previous set of functions, but for hp objects.
   */
  template <int dim, int spacedim>
  void
  create_point_source_vector(
    const hp::MappingCollection<dim, spacedim> &mapping,
    const hp::DoFHandler<dim, spacedim> &       dof_handler,
    const Point<spacedim> &                     p,
    const Point<dim> &                          direction,
    Vector<double> &                            rhs_vector);

  /**
   * Like the previous set of functions, but for hp objects. The function uses
   * an implied default $Q_1$ mapping object. Note that if your hp::DoFHandler
   * uses any active fe index other than zero, then you need to call the
   * function above that provides a mapping object for each active fe index.
   */
  template <int dim, int spacedim>
  void
  create_point_source_vector(const hp::DoFHandler<dim, spacedim> &dof_handler,
                             const Point<spacedim> &              p,
                             const Point<dim> &                   direction,
                             Vector<double> &                     rhs_vector);

  /**
   * Create a right hand side vector from boundary forces. Prior content of
   * the given @p rhs_vector vector is deleted.
   *
   * See the general documentation of this namespace for further information.
   *
   * @see
   * @ref GlossBoundaryIndicator "Glossary entry on boundary indicators"
   */
  template <int dim, int spacedim, typename VectorType>
  void
  create_boundary_right_hand_side(
    const Mapping<dim, spacedim> &                             mapping,
    const DoFHandler<dim, spacedim> &                          dof,
    const Quadrature<dim - 1> &                                q,
    const Function<spacedim, typename VectorType::value_type> &rhs,
    VectorType &                                               rhs_vector,
    const std::set<types::boundary_id> &                       boundary_ids =
      std::set<types::boundary_id>());

  /**
   * Call the create_boundary_right_hand_side() function, see above, with
   * <tt>mapping=MappingQGeneric@<dim@>(1)</tt>.
   *
   * @see
   * @ref GlossBoundaryIndicator "Glossary entry on boundary indicators"
   */
  template <int dim, int spacedim, typename VectorType>
  void
  create_boundary_right_hand_side(
    const DoFHandler<dim, spacedim> &                          dof,
    const Quadrature<dim - 1> &                                q,
    const Function<spacedim, typename VectorType::value_type> &rhs,
    VectorType &                                               rhs_vector,
    const std::set<types::boundary_id> &                       boundary_ids =
      std::set<types::boundary_id>());

  /**
   * Same as the set of functions above, but for hp objects.
   *
   * @see
   * @ref GlossBoundaryIndicator "Glossary entry on boundary indicators"
   */
  template <int dim, int spacedim, typename VectorType>
  void
  create_boundary_right_hand_side(
    const hp::MappingCollection<dim, spacedim> &               mapping,
    const hp::DoFHandler<dim, spacedim> &                      dof,
    const hp::QCollection<dim - 1> &                           q,
    const Function<spacedim, typename VectorType::value_type> &rhs,
    VectorType &                                               rhs_vector,
    const std::set<types::boundary_id> &                       boundary_ids =
      std::set<types::boundary_id>());

  /**
   * Call the create_boundary_right_hand_side() function, see above, with a
   * single Q1 mapping as collection. This function therefore will only work
   * if the only active fe index in use is zero.
   *
   * @see
   * @ref GlossBoundaryIndicator "Glossary entry on boundary indicators"
   */
  template <int dim, int spacedim, typename VectorType>
  void
  create_boundary_right_hand_side(
    const hp::DoFHandler<dim, spacedim> &                      dof,
    const hp::QCollection<dim - 1> &                           q,
    const Function<spacedim, typename VectorType::value_type> &rhs,
    VectorType &                                               rhs_vector,
    const std::set<types::boundary_id> &                       boundary_ids =
      std::set<types::boundary_id>());

  //@}
  /**
   * @name Evaluation of functions and errors
   */
  //@{

  /**
   * Compute the cellwise error of the finite element solution.  Integrate the
   * difference between a reference function which is given as a continuous
   * function object, and a finite element function. The result of this
   * function is the vector @p difference that contains one value per active
   * cell $K$ of the triangulation. Each of the values of this vector $d$
   * equals
   * @f{align*}{
   * d_K = \| u-u_h \|_X
   * @f}
   * where $X$ denotes the norm chosen and $u$ represents the exact solution.
   *
   * It is assumed that the number of components of the function @p
   * exact_solution matches that of the finite element used by @p dof.
   *
   * To compute a global error norm of a finite element solution, use
   * VectorTools::compute_global_error() with the output vector computed with
   * this function.
   *
   * @param[in] mapping The mapping that is used when integrating the
   * difference $u-u_h$.
   * @param[in] dof The DoFHandler object that describes the finite element
   * space in which the solution vector lives.
   * @param[in] fe_function A vector with nodal values representing the
   * numerical approximation $u_h$. This vector needs to correspond to the
   * finite element space represented by @p dof.
   * @param[in] exact_solution The exact solution that is used to compute the
   * error.
   * @param[out] difference The vector of values $d_K$ computed as above.
   * @param[in] q The quadrature formula used to approximate the integral
   * shown above. Note that some quadrature formulas are more useful than
   * other in integrating $u-u_h$. For example, it is known that the $Q_1$
   * approximation $u_h$ to the exact solution $u$ of a Laplace equation is
   * particularly accurate (in fact, superconvergent, i.e. accurate to higher
   * order) at the 4 Gauss points of a cell in 2d (or 8 points in 3d) that
   * correspond to a QGauss(2) object. Consequently, because a QGauss(2)
   * formula only evaluates the two solutions at these particular points,
   * choosing this quadrature formula may indicate an error far smaller than
   * it actually is.
   * @param[in] norm The norm $X$ shown above that should be computed. If the
   * norm is NormType::Hdiv_seminorm, then the finite element on which this
   * function is called needs to have at least dim vector components, and the
   * divergence will be computed on the first div components. This works, for
   * example, on the finite elements used for the mixed Laplace (step-20) and
   * the Stokes equations (step-22).
   * @param[in] weight The additional argument @p weight allows to evaluate
   * weighted norms.  The weight function may be scalar, establishing a
   * spatially variable weight in the domain for all components equally. This
   * may be used, for instance, to only integrate over parts of the domain.
   * The weight function may also be vector-valued, with as many components as
   * the finite element: Then, different components get different weights. A
   * typical application is when the error with respect to only one or a
   * subset of the solution variables is to be computed, in which case the
   * other components would have weight values equal to zero. The
   * ComponentSelectFunction class is particularly useful for this purpose as
   * it provides such a "mask" weight. The weight function is expected to be
   * positive, but negative values are not filtered. The default value of this
   * function, a null pointer, is interpreted as "no weighting function",
   * i.e., weight=1 in the whole domain for all vector components uniformly.
   * @param[in] exponent This value denotes the $p$ used in computing
   * $L^p$-norms and $W^{1,p}$-norms. The value is ignored if a @p norm other
   * than NormType::Lp_norm, NormType::W1p_norm, or NormType::W1p_seminorm
   * is chosen.
   *
   *
   * See the general documentation of this namespace for more information.
   *
   * @note If the integration here happens over the cells of a
   * parallel::distribute::Triangulation object, then this function computes
   * the vector elements $d_K$ for an output vector with as many cells as
   * there are active cells of the triangulation object of the current
   * processor. However, not all active cells are in fact locally owned: some
   * may be ghost or artificial cells (see
   * @ref GlossGhostCell "here"
   * and
   * @ref GlossArtificialCell "here").
   * The vector computed will, in the case of a distributed triangulation,
   * contain zeros for cells that are not locally owned. As a consequence, in
   * order to compute the <i>global</i> $L_2$ error (for example), the errors
   * from different processors need to be combined, see
   * VectorTools::compute_global_error().
   *
   * Instantiations for this template are provided for some vector types (see
   * the general documentation of the namespace), but only for InVectors as in
   * the documentation of the namespace, OutVector only Vector<double> and
   * Vector<float>.
   */
  template <int dim, class InVector, class OutVector, int spacedim>
  void
  integrate_difference(const Mapping<dim, spacedim> &    mapping,
                       const DoFHandler<dim, spacedim> & dof,
                       const InVector &                  fe_function,
                       const Function<spacedim, double> &exact_solution,
                       OutVector &                       difference,
                       const Quadrature<dim> &           q,
                       const NormType &                  norm,
                       const Function<spacedim, double> *weight   = nullptr,
                       const double                      exponent = 2.);

  /**
   * Call the integrate_difference() function, see above, with
   * <tt>mapping=MappingQGeneric@<dim@>(1)</tt>.
   */
  template <int dim, class InVector, class OutVector, int spacedim>
  void
  integrate_difference(const DoFHandler<dim, spacedim> & dof,
                       const InVector &                  fe_function,
                       const Function<spacedim, double> &exact_solution,
                       OutVector &                       difference,
                       const Quadrature<dim> &           q,
                       const NormType &                  norm,
                       const Function<spacedim, double> *weight   = nullptr,
                       const double                      exponent = 2.);

  /**
   * Same as above for hp.
   */
  template <int dim, class InVector, class OutVector, int spacedim>
  void
  integrate_difference(const hp::MappingCollection<dim, spacedim> &mapping,
                       const hp::DoFHandler<dim, spacedim> &       dof,
                       const InVector &                            fe_function,
                       const Function<spacedim, double> &exact_solution,
                       OutVector &                       difference,
                       const hp::QCollection<dim> &      q,
                       const NormType &                  norm,
                       const Function<spacedim, double> *weight   = nullptr,
                       const double                      exponent = 2.);

  /**
   * Call the integrate_difference() function, see above, with
   * <tt>mapping=MappingQGeneric@<dim@>(1)</tt>.
   */
  template <int dim, class InVector, class OutVector, int spacedim>
  void
  integrate_difference(const hp::DoFHandler<dim, spacedim> &dof,
                       const InVector &                     fe_function,
                       const Function<spacedim, double> &   exact_solution,
                       OutVector &                          difference,
                       const hp::QCollection<dim> &         q,
                       const NormType &                     norm,
                       const Function<spacedim, double> *   weight   = nullptr,
                       const double                         exponent = 2.);

  /**
   * Take a Vector @p cellwise_error of errors on each cell with
   * <tt>tria.n_active_cells()</tt> entries and return the global
   * error as given by @p norm.
   *
   * The @p cellwise_error vector is typically an output produced by
   * VectorTools::integrate_difference() and you normally want to supply the
   * same value for @p norm as you used in VectorTools::integrate_difference().
   *
   * If the given Triangulation is a parallel::Triangulation, entries
   * in @p cellwise_error that do not correspond to locally owned cells are
   * assumed to be 0.0 and a parallel reduction using MPI is done to compute
   * the global error.
   *
   * @param tria The Triangulation with active cells corresponding with the
   * entries in @p cellwise_error.
   * @param cellwise_error Vector of errors on each active cell.
   * @param norm The type of norm to compute.
   * @param exponent The exponent $p$ to use for $L^p$-norms and
   * $W^{1,p}$-norms. The value is ignored if a @p norm other
   * than NormType::Lp_norm, NormType::W1p_norm, or NormType::W1p_seminorm
   * is chosen.
   *
   * @note Instantiated for type Vector<double> and Vector<float>.
   */
  template <int dim, int spacedim, class InVector>
  double
  compute_global_error(const Triangulation<dim, spacedim> &tria,
                       const InVector &                    cellwise_error,
                       const NormType &                    norm,
                       const double                        exponent = 2.);

  /**
   * Point error evaluation. Find the first cell containing the given point
   * and compute the difference of a (possibly vector-valued) finite element
   * function and a continuous function (with as many vector components as the
   * finite element) at this point.
   *
   * This is a wrapper function using a Q1-mapping for cell boundaries to call
   * the other point_difference() function.
   *
   * @note If the cell in which the point is found is not locally owned, an
   * exception of type VectorTools::ExcPointNotAvailableHere is thrown.
   */
  template <int dim, typename VectorType, int spacedim>
  void
  point_difference(
    const DoFHandler<dim, spacedim> &                          dof,
    const VectorType &                                         fe_function,
    const Function<spacedim, typename VectorType::value_type> &exact_solution,
    Vector<typename VectorType::value_type> &                  difference,
    const Point<spacedim> &                                    point);

  /**
   * Point error evaluation. Find the first cell containing the given point
   * and compute the difference of a (possibly vector-valued) finite element
   * function and a continuous function (with as many vector components as the
   * finite element) at this point.
   *
   * Compared with the other function of the same name, this function uses an
   * arbitrary mapping to evaluate the difference.
   *
   * @note If the cell in which the point is found is not locally owned, an
   * exception of type VectorTools::ExcPointNotAvailableHere is thrown.
   */
  template <int dim, typename VectorType, int spacedim>
  void
  point_difference(
    const Mapping<dim, spacedim> &                             mapping,
    const DoFHandler<dim, spacedim> &                          dof,
    const VectorType &                                         fe_function,
    const Function<spacedim, typename VectorType::value_type> &exact_solution,
    Vector<typename VectorType::value_type> &                  difference,
    const Point<spacedim> &                                    point);

  /**
   * Evaluate a possibly vector-valued finite element function defined by the
   * given DoFHandler and nodal vector @p fe_function at the given point @p
   * point, and return the (vector) value of this function through the last
   * argument.
   *
   * This function uses a $Q_1$-mapping for the cell the point is evaluated
   * in. If you need to evaluate using a different mapping (for example when
   * using curved boundaries), use the point_difference() function that takes
   * a mapping.
   *
   * This function is not particularly cheap. This is because it first
   * needs to find which cell a given point is in, then find the point
   * on the reference cell that matches the given evaluation point,
   * and then evaluate the shape functions there. You probably do not
   * want to use this function to evaluate the solution at <i>many</i>
   * points. For this kind of application, the FEFieldFunction class
   * offers at least some optimizations. On the other hand, if you
   * want to evaluate <i>many solutions</i> at the same point, you may
   * want to look at the VectorTools::create_point_source_vector()
   * function.
   *
   * @note If the cell in which the point is found is not locally owned, an
   *   exception of type VectorTools::ExcPointNotAvailableHere is thrown.
   *
   * @note This function needs to find the cell within which a point lies,
   *   and this can only be done up to a certain numerical tolerance of course.
   *   Consequently, for points that are on, or close to, the boundary of
   *   a cell, you may get the value of the finite element field either
   *   here or there, depending on which cell the point is found in. This
   *   does not matter (to within the same tolerance) if the finite element
   *   field is continuous. On the other hand, if the finite element in use
   *   is <i>not</i> continuous, then you will get unpredictable values for
   *   points on or close to the boundary of the cell, as one would expect
   *   when trying to evaluate point values of discontinuous functions.
   */
  template <int dim, typename VectorType, int spacedim>
  void
  point_value(const DoFHandler<dim, spacedim> &        dof,
              const VectorType &                       fe_function,
              const Point<spacedim> &                  point,
              Vector<typename VectorType::value_type> &value);

  /**
   * Same as above for hp.
   *
   * @note If the cell in which the point is found is not locally owned, an
   * exception of type VectorTools::ExcPointNotAvailableHere is thrown.
   *
   * @note This function needs to find the cell within which a point lies,
   *   and this can only be done up to a certain numerical tolerance of course.
   *   Consequently, for points that are on, or close to, the boundary of
   *   a cell, you may get the value of the finite element field either
   *   here or there, depending on which cell the point is found in. This
   *   does not matter (to within the same tolerance) if the finite element
   *   field is continuous. On the other hand, if the finite element in use
   *   is <i>not</i> continuous, then you will get unpredictable values for
   *   points on or close to the boundary of the cell, as one would expect
   *   when trying to evaluate point values of discontinuous functions.
   */
  template <int dim, typename VectorType, int spacedim>
  void
  point_value(const hp::DoFHandler<dim, spacedim> &    dof,
              const VectorType &                       fe_function,
              const Point<spacedim> &                  point,
              Vector<typename VectorType::value_type> &value);

  /**
   * Evaluate a scalar finite element function defined by the given DoFHandler
   * and nodal vector @p fe_function at the given point @p point, and return
   * the value of this function.
   *
   * This function uses a Q1-mapping for the cell the point is evaluated
   * in. If you need to evaluate using a different mapping (for example when
   * using curved boundaries), use the point_difference() function that takes
   * a mapping.
   *
   * This function is not particularly cheap. This is because it first
   * needs to find which cell a given point is in, then find the point
   * on the reference cell that matches the given evaluation point,
   * and then evaluate the shape functions there. You probably do not
   * want to use this function to evaluate the solution at <i>many</i>
   * points. For this kind of application, the FEFieldFunction class
   * offers at least some optimizations. On the other hand, if you
   * want to evaluate <i>many solutions</i> at the same point, you may
   * want to look at the VectorTools::create_point_source_vector()
   * function.
   *
   * This function is used in the "Possibilities for extensions" part of the
   * results section of
   * @ref step_3 "step-3".
   *
   * @note If the cell in which the point is found is not locally owned, an
   * exception of type VectorTools::ExcPointNotAvailableHere is thrown.
   *
   * @note This function needs to find the cell within which a point lies,
   *   and this can only be done up to a certain numerical tolerance of course.
   *   Consequently, for points that are on, or close to, the boundary of
   *   a cell, you may get the value of the finite element field either
   *   here or there, depending on which cell the point is found in. This
   *   does not matter (to within the same tolerance) if the finite element
   *   field is continuous. On the other hand, if the finite element in use
   *   is <i>not</i> continuous, then you will get unpredictable values for
   *   points on or close to the boundary of the cell, as one would expect
   *   when trying to evaluate point values of discontinuous functions.
   */
  template <int dim, typename VectorType, int spacedim>
  typename VectorType::value_type
  point_value(const DoFHandler<dim, spacedim> &dof,
              const VectorType &               fe_function,
              const Point<spacedim> &          point);

  /**
   * Same as above for hp.
   *
   * @note If the cell in which the point is found is not locally owned, an
   * exception of type VectorTools::ExcPointNotAvailableHere is thrown.
   *
   * @note This function needs to find the cell within which a point lies,
   *   and this can only be done up to a certain numerical tolerance of course.
   *   Consequently, for points that are on, or close to, the boundary of
   *   a cell, you may get the value of the finite element field either
   *   here or there, depending on which cell the point is found in. This
   *   does not matter (to within the same tolerance) if the finite element
   *   field is continuous. On the other hand, if the finite element in use
   *   is <i>not</i> continuous, then you will get unpredictable values for
   *   points on or close to the boundary of the cell, as one would expect
   *   when trying to evaluate point values of discontinuous functions.
   */
  template <int dim, typename VectorType, int spacedim>
  typename VectorType::value_type
  point_value(const hp::DoFHandler<dim, spacedim> &dof,
              const VectorType &                   fe_function,
              const Point<spacedim> &              point);

  /**
   * Evaluate a possibly vector-valued finite element function defined by the
   * given DoFHandler and nodal vector @p fe_function at the given point @p
   * point, and return the (vector) value of this function through the last
   * argument.
   *
   * Compared with the other function of the same name, this function uses an
   * arbitrary mapping to evaluate the point value.
   *
   * This function is not particularly cheap. This is because it first
   * needs to find which cell a given point is in, then find the point
   * on the reference cell that matches the given evaluation point,
   * and then evaluate the shape functions there. You probably do not
   * want to use this function to evaluate the solution at <i>many</i>
   * points. For this kind of application, the FEFieldFunction class
   * offers at least some optimizations. On the other hand, if you
   * want to evaluate <i>many solutions</i> at the same point, you may
   * want to look at the VectorTools::create_point_source_vector()
   * function.
   *
   * @note If the cell in which the point is found is not locally owned, an
   * exception of type VectorTools::ExcPointNotAvailableHere is thrown.
   *
   * @note This function needs to find the cell within which a point lies,
   *   and this can only be done up to a certain numerical tolerance of course.
   *   Consequently, for points that are on, or close to, the boundary of
   *   a cell, you may get the value of the finite element field either
   *   here or there, depending on which cell the point is found in. This
   *   does not matter (to within the same tolerance) if the finite element
   *   field is continuous. On the other hand, if the finite element in use
   *   is <i>not</i> continuous, then you will get unpredictable values for
   *   points on or close to the boundary of the cell, as one would expect
   *   when trying to evaluate point values of discontinuous functions.
   */
  template <int dim, typename VectorType, int spacedim>
  void
  point_value(const Mapping<dim, spacedim> &           mapping,
              const DoFHandler<dim, spacedim> &        dof,
              const VectorType &                       fe_function,
              const Point<spacedim> &                  point,
              Vector<typename VectorType::value_type> &value);

  /**
   * Same as above for hp.
   *
   * @note If the cell in which the point is found is not locally owned, an
   * exception of type VectorTools::ExcPointNotAvailableHere is thrown.
   *
   * @note This function needs to find the cell within which a point lies,
   *   and this can only be done up to a certain numerical tolerance of course.
   *   Consequently, for points that are on, or close to, the boundary of
   *   a cell, you may get the value of the finite element field either
   *   here or there, depending on which cell the point is found in. This
   *   does not matter (to within the same tolerance) if the finite element
   *   field is continuous. On the other hand, if the finite element in use
   *   is <i>not</i> continuous, then you will get unpredictable values for
   *   points on or close to the boundary of the cell, as one would expect
   *   when trying to evaluate point values of discontinuous functions.
   */
  template <int dim, typename VectorType, int spacedim>
  void
  point_value(const hp::MappingCollection<dim, spacedim> &mapping,
              const hp::DoFHandler<dim, spacedim> &       dof,
              const VectorType &                          fe_function,
              const Point<spacedim> &                     point,
              Vector<typename VectorType::value_type> &   value);

  /**
   * Evaluate a scalar finite element function defined by the given DoFHandler
   * and nodal vector @p fe_function at the given point @p point, and return
   * the value of this function.
   *
   * Compared with the other function of the same name, this function uses an
   * arbitrary mapping to evaluate the difference.
   *
   * This function is not particularly cheap. This is because it first
   * needs to find which cell a given point is in, then find the point
   * on the reference cell that matches the given evaluation point,
   * and then evaluate the shape functions there. You probably do not
   * want to use this function to evaluate the solution at <i>many</i>
   * points. For this kind of application, the FEFieldFunction class
   * offers at least some optimizations. On the other hand, if you
   * want to evaluate <i>many solutions</i> at the same point, you may
   * want to look at the VectorTools::create_point_source_vector()
   * function.
   *
   * @note If the cell in which the point is found is not locally owned, an
   * exception of type VectorTools::ExcPointNotAvailableHere is thrown.
   *
   * @note This function needs to find the cell within which a point lies,
   *   and this can only be done up to a certain numerical tolerance of course.
   *   Consequently, for points that are on, or close to, the boundary of
   *   a cell, you may get the value of the finite element field either
   *   here or there, depending on which cell the point is found in. This
   *   does not matter (to within the same tolerance) if the finite element
   *   field is continuous. On the other hand, if the finite element in use
   *   is <i>not</i> continuous, then you will get unpredictable values for
   *   points on or close to the boundary of the cell, as one would expect
   *   when trying to evaluate point values of discontinuous functions.
   */
  template <int dim, typename VectorType, int spacedim>
  typename VectorType::value_type
  point_value(const Mapping<dim, spacedim> &   mapping,
              const DoFHandler<dim, spacedim> &dof,
              const VectorType &               fe_function,
              const Point<spacedim> &          point);

  /**
   * Same as above for hp.
   *
   * @note If the cell in which the point is found is not locally owned, an
   * exception of type VectorTools::ExcPointNotAvailableHere is thrown.
   *
   * @note This function needs to find the cell within which a point lies,
   *   and this can only be done up to a certain numerical tolerance of course.
   *   Consequently, for points that are on, or close to, the boundary of
   *   a cell, you may get the value of the finite element field either
   *   here or there, depending on which cell the point is found in. This
   *   does not matter (to within the same tolerance) if the finite element
   *   field is continuous. On the other hand, if the finite element in use
   *   is <i>not</i> continuous, then you will get unpredictable values for
   *   points on or close to the boundary of the cell, as one would expect
   *   when trying to evaluate point values of discontinuous functions.
   */
  template <int dim, typename VectorType, int spacedim>
  typename VectorType::value_type
  point_value(const hp::MappingCollection<dim, spacedim> &mapping,
              const hp::DoFHandler<dim, spacedim> &       dof,
              const VectorType &                          fe_function,
              const Point<spacedim> &                     point);

  /**
   * Evaluate a possibly vector-valued finite element function defined by the
   * given DoFHandler and nodal vector at the given point, and return the
   * (vector) gradient of this function through the last argument.
   *
   * This is a wrapper function using a Q1-mapping for cell boundaries to call
   * the other point_gradient() function.
   *
   * This function is not particularly cheap. This is because it first
   * needs to find which cell a given point is in, then find the point
   * on the reference cell that matches the given evaluation point,
   * and then evaluate the shape functions there. You probably do not
   * want to use this function to evaluate the solution at <i>many</i>
   * points. For this kind of application, the FEFieldFunction class
   * offers at least some optimizations. On the other hand, if you
   * want to evaluate <i>many solutions</i> at the same point, you may
   * want to look at the VectorTools::create_point_source_vector()
   * function.
   *
   * @note If the cell in which the point is found is not locally owned, an
   * exception of type VectorTools::ExcPointNotAvailableHere is thrown.
   *
   * @note This function needs to find the cell within which a point lies,
   *   and this can only be done up to a certain numerical tolerance of course.
   *   Consequently, for points that are on, or close to, the boundary of
   *   a cell, you may get the gradient of the finite element field either
   *   here or there, depending on which cell the point is found in. Since
   *   the gradient is, for most elements, discontinuous from one cell or
   *   the other, you will get unpredictable values for
   *   points on or close to the boundary of the cell, as one would expect
   *   when trying to evaluate point values of discontinuous functions.
   */
  template <int dim, typename VectorType, int spacedim>
  void
  point_gradient(
    const DoFHandler<dim, spacedim> &dof,
    const VectorType &               fe_function,
    const Point<spacedim> &          point,
    std::vector<Tensor<1, spacedim, typename VectorType::value_type>> &value);

  /**
   * Same as above for hp.
   *
   * @note If the cell in which the point is found is not locally owned, an
   * exception of type VectorTools::ExcPointNotAvailableHere is thrown.
   *
   * @note This function needs to find the cell within which a point lies,
   *   and this can only be done up to a certain numerical tolerance of course.
   *   Consequently, for points that are on, or close to, the boundary of
   *   a cell, you may get the gradient of the finite element field either
   *   here or there, depending on which cell the point is found in. Since
   *   the gradient is, for most elements, discontinuous from one cell or
   *   the other, you will get unpredictable values for
   *   points on or close to the boundary of the cell, as one would expect
   *   when trying to evaluate point values of discontinuous functions.
   */
  template <int dim, typename VectorType, int spacedim>
  void
  point_gradient(
    const hp::DoFHandler<dim, spacedim> &dof,
    const VectorType &                   fe_function,
    const Point<spacedim> &              point,
    std::vector<Tensor<1, spacedim, typename VectorType::value_type>> &value);

  /**
   * Evaluate a scalar finite element function defined by the given DoFHandler
   * and nodal vector at the given point, and return the gradient of this
   * function.
   *
   * Compared with the other function of the same name, this is a wrapper
   * function using a Q1-mapping for cells.
   *
   * This function is not particularly cheap. This is because it first
   * needs to find which cell a given point is in, then find the point
   * on the reference cell that matches the given evaluation point,
   * and then evaluate the shape functions there. You probably do not
   * want to use this function to evaluate the solution at <i>many</i>
   * points. For this kind of application, the FEFieldFunction class
   * offers at least some optimizations. On the other hand, if you
   * want to evaluate <i>many solutions</i> at the same point, you may
   * want to look at the VectorTools::create_point_source_vector()
   * function.
   *
   * @note If the cell in which the point is found is not locally owned, an
   * exception of type VectorTools::ExcPointNotAvailableHere is thrown.
   *
   * @note This function needs to find the cell within which a point lies,
   *   and this can only be done up to a certain numerical tolerance of course.
   *   Consequently, for points that are on, or close to, the boundary of
   *   a cell, you may get the gradient of the finite element field either
   *   here or there, depending on which cell the point is found in. Since
   *   the gradient is, for most elements, discontinuous from one cell or
   *   the other, you will get unpredictable values for
   *   points on or close to the boundary of the cell, as one would expect
   *   when trying to evaluate point values of discontinuous functions.
   */
  template <int dim, typename VectorType, int spacedim>
  Tensor<1, spacedim, typename VectorType::value_type>
  point_gradient(const DoFHandler<dim, spacedim> &dof,
                 const VectorType &               fe_function,
                 const Point<spacedim> &          point);

  /**
   * Same as above for hp.
   *
   * @note If the cell in which the point is found is not locally owned, an
   * exception of type VectorTools::ExcPointNotAvailableHere is thrown.
   *
   * @note This function needs to find the cell within which a point lies,
   *   and this can only be done up to a certain numerical tolerance of course.
   *   Consequently, for points that are on, or close to, the boundary of
   *   a cell, you may get the gradient of the finite element field either
   *   here or there, depending on which cell the point is found in. Since
   *   the gradient is, for most elements, discontinuous from one cell or
   *   the other, you will get unpredictable values for
   *   points on or close to the boundary of the cell, as one would expect
   *   when trying to evaluate point values of discontinuous functions.
   */
  template <int dim, typename VectorType, int spacedim>
  Tensor<1, spacedim, typename VectorType::value_type>
  point_gradient(const hp::DoFHandler<dim, spacedim> &dof,
                 const VectorType &                   fe_function,
                 const Point<spacedim> &              point);

  /**
   * Evaluate a possibly vector-valued finite element function defined by the
   * given DoFHandler and nodal vector at the given point, and return the
   * gradients of this function through the last argument.
   *
   * Compared with the other function of the same name, this function uses an
   * arbitrary mapping for evaluation.
   *
   * This function is not particularly cheap. This is because it first
   * needs to find which cell a given point is in, then find the point
   * on the reference cell that matches the given evaluation point,
   * and then evaluate the shape functions there. You probably do not
   * want to use this function to evaluate the solution at <i>many</i>
   * points. For this kind of application, the FEFieldFunction class
   * offers at least some optimizations. On the other hand, if you
   * want to evaluate <i>many solutions</i> at the same point, you may
   * want to look at the VectorTools::create_point_source_vector()
   * function.
   *
   * @note If the cell in which the point is found is not locally owned, an
   * exception of type VectorTools::ExcPointNotAvailableHere is thrown.
   *
   * @note This function needs to find the cell within which a point lies,
   *   and this can only be done up to a certain numerical tolerance of course.
   *   Consequently, for points that are on, or close to, the boundary of
   *   a cell, you may get the gradient of the finite element field either
   *   here or there, depending on which cell the point is found in. Since
   *   the gradient is, for most elements, discontinuous from one cell or
   *   the other, you will get unpredictable values for
   *   points on or close to the boundary of the cell, as one would expect
   *   when trying to evaluate point values of discontinuous functions.
   */
  template <int dim, typename VectorType, int spacedim>
  void
  point_gradient(
    const Mapping<dim, spacedim> &   mapping,
    const DoFHandler<dim, spacedim> &dof,
    const VectorType &               fe_function,
    const Point<spacedim> &          point,
    std::vector<Tensor<1, spacedim, typename VectorType::value_type>> &value);

  /**
   * Same as above for hp.
   *
   * @note If the cell in which the point is found is not locally owned, an
   * exception of type VectorTools::ExcPointNotAvailableHere is thrown.
   *
   * @note This function needs to find the cell within which a point lies,
   *   and this can only be done up to a certain numerical tolerance of course.
   *   Consequently, for points that are on, or close to, the boundary of
   *   a cell, you may get the gradient of the finite element field either
   *   here or there, depending on which cell the point is found in. Since
   *   the gradient is, for most elements, discontinuous from one cell or
   *   the other, you will get unpredictable values for
   *   points on or close to the boundary of the cell, as one would expect
   *   when trying to evaluate point values of discontinuous functions.
   */
  template <int dim, typename VectorType, int spacedim>
  void
  point_gradient(
    const hp::MappingCollection<dim, spacedim> &mapping,
    const hp::DoFHandler<dim, spacedim> &       dof,
    const VectorType &                          fe_function,
    const Point<spacedim> &                     point,
    std::vector<Tensor<1, spacedim, typename VectorType::value_type>> &value);

  /**
   * Evaluate a scalar finite element function defined by the given DoFHandler
   * and nodal vector at the given point, and return the gradient of this
   * function.
   *
   * Compared with the other function of the same name, this function uses an
   * arbitrary mapping for evaluation.
   *
   * This function is not particularly cheap. This is because it first
   * needs to find which cell a given point is in, then find the point
   * on the reference cell that matches the given evaluation point,
   * and then evaluate the shape functions there. You probably do not
   * want to use this function to evaluate the solution at <i>many</i>
   * points. For this kind of application, the FEFieldFunction class
   * offers at least some optimizations. On the other hand, if you
   * want to evaluate <i>many solutions</i> at the same point, you may
   * want to look at the VectorTools::create_point_source_vector()
   * function.
   *
   * @note If the cell in which the point is found is not locally owned, an
   * exception of type VectorTools::ExcPointNotAvailableHere is thrown.
   *
   * @note This function needs to find the cell within which a point lies,
   *   and this can only be done up to a certain numerical tolerance of course.
   *   Consequently, for points that are on, or close to, the boundary of
   *   a cell, you may get the gradient of the finite element field either
   *   here or there, depending on which cell the point is found in. Since
   *   the gradient is, for most elements, discontinuous from one cell or
   *   the other, you will get unpredictable values for
   *   points on or close to the boundary of the cell, as one would expect
   *   when trying to evaluate point values of discontinuous functions.
   */
  template <int dim, typename VectorType, int spacedim>
  Tensor<1, spacedim, typename VectorType::value_type>
  point_gradient(const Mapping<dim, spacedim> &   mapping,
                 const DoFHandler<dim, spacedim> &dof,
                 const VectorType &               fe_function,
                 const Point<spacedim> &          point);

  /**
   * Same as above for hp.
   *
   * @note If the cell in which the point is found is not locally owned, an
   * exception of type VectorTools::ExcPointNotAvailableHere is thrown.
   *
   * @note This function needs to find the cell within which a point lies,
   *   and this can only be done up to a certain numerical tolerance of course.
   *   Consequently, for points that are on, or close to, the boundary of
   *   a cell, you may get the gradient of the finite element field either
   *   here or there, depending on which cell the point is found in. Since
   *   the gradient is, for most elements, discontinuous from one cell or
   *   the other, you will get unpredictable values for
   *   points on or close to the boundary of the cell, as one would expect
   *   when trying to evaluate point values of discontinuous functions.
   */
  template <int dim, typename VectorType, int spacedim>
  Tensor<1, spacedim, typename VectorType::value_type>
  point_gradient(const hp::MappingCollection<dim, spacedim> &mapping,
                 const hp::DoFHandler<dim, spacedim> &       dof,
                 const VectorType &                          fe_function,
                 const Point<spacedim> &                     point);

  //@}
  /**
   * Mean value operations
   */
  //@{

  /**
   * Subtract the (algebraic) mean value from a vector.
   *
   * This function is most frequently used as a mean-value filter for Stokes:
   * The pressure in Stokes' equations with only Dirichlet boundaries for the
   * velocities is only determined up to a constant. This function allows to
   * subtract the mean value of the pressure. It is usually called in a
   * preconditioner and generates updates with mean value zero. The mean value
   * is computed as the mean value of the degrees of freedom values as given
   * by the input vector; they are not weighted by the area of cells, i.e. the
   * mean is computed as $\sum_i v_i$, rather than as $\int_\Omega v(x) =
   * \int_\Omega \sum_i v_i \phi_i(x)$. The latter can be obtained from the
   * VectorTools::compute_mean_function, however.
   *
   * Apart from the vector @p v to operate on, this function takes a boolean
   * mask @p p_select that has a true entry for every element of the vector
   * for which the mean value shall be computed and later subtracted. The
   * argument is used to denote which components of the solution vector
   * correspond to the pressure, and avoid touching all other components of
   * the vector, such as the velocity components. (Note, however, that the
   * mask is not a
   * @ref GlossComponentMask
   * operating on the vector components of the finite element the solution
   * vector @p v may be associated with; rather, it is a mask on the entire
   * vector, without reference to what the vector elements mean.)
   *
   * The boolean mask @p p_select has an empty vector as default value, which
   * will be interpreted as selecting all vector elements, hence, subtracting
   * the algebraic mean value on the whole vector. This allows to call this
   * function without a boolean mask if the whole vector should be processed.
   *
   * @note In the context of using this function to filter out the kernel of
   * an operator (such as the null space of the Stokes operator that consists
   * of the constant pressures), this function only makes sense for finite
   * elements for which the null space indeed consists of the vector
   * $(1,1,\ldots,1)^T$. This is the case for example for the usual Lagrange
   * elements where the sum of all shape functions equals the function that is
   * constant one. However, it is not true for some other functions: for
   * example, for the FE_DGP element (another valid choice for the pressure in
   * Stokes discretizations), the first shape function on each cell is
   * constant while further elements are $L_2$ orthogonal to it (on the
   * reference cell); consequently, the sum of all shape functions is not
   * equal to one, and the vector that is associated with the constant mode is
   * not equal to $(1,1,\ldots,1)^T$. For such elements, a different procedure
   * has to be used when subtracting the mean value.
   *
   * @warning This function can only be used for distributed vector classes
   * provided the boolean mask is empty, i.e. selecting the whole vector.
   */
  template <typename VectorType>
  void
  subtract_mean_value(VectorType &v, const std::vector<bool> &p_select = {});


  /**
   * Compute the mean value of one component of the solution.
   *
   * This function integrates the chosen component over the whole domain and
   * returns the result, i.e. it computes $\frac{1}{|\Omega|}\int_\Omega
   * [u_h(x)]_c \; dx$ where $c$ is the vector component and $u_h$ is the
   * function representation of the nodal vector given as fourth argument. The
   * integral is evaluated numerically using the quadrature formula given as
   * third argument.
   *
   * This function is used in the "Possibilities for extensions" part of the
   * results section of
   * @ref step_3 "step-3".
   *
   * @note The function is most often used when solving a problem whose
   * solution is only defined up to a constant, for example a pure Neumann
   * problem or the pressure in a Stokes or Navier-Stokes problem. In both
   * cases, subtracting the mean value as computed by the current function,
   * from the nodal vector does not generally yield the desired result of a
   * finite element function with mean value zero. In fact, it only works for
   * Lagrangian elements. For all other elements, you will need to compute the
   * mean value and subtract it right inside the evaluation routine.
   */
  template <int dim, typename VectorType, int spacedim>
  typename VectorType::value_type
  compute_mean_value(const Mapping<dim, spacedim> &   mapping,
                     const DoFHandler<dim, spacedim> &dof,
                     const Quadrature<dim> &          quadrature,
                     const VectorType &               v,
                     const unsigned int               component);

  /**
   * Call the other compute_mean_value() function, see above, with
   * <tt>mapping=MappingQGeneric@<dim@>(1)</tt>.
   */
  template <int dim, typename VectorType, int spacedim>
  typename VectorType::value_type
  compute_mean_value(const DoFHandler<dim, spacedim> &dof,
                     const Quadrature<dim> &          quadrature,
                     const VectorType &               v,
                     const unsigned int               component);
  //@}
  /**
   * Geometrical interpolation
   */
  //@{
  /**
   * Given a DoFHandler containing at least a spacedim vector field, this
   * function interpolates the Triangulation at the support points of a FE_Q()
   * finite element of the same degree as the degree of the required
   * components.
   *
   * Curved manifold are respected, and the resulting VectorType will be
   * geometrically consistent. The resulting map is guaranteed to be
   * interpolatory at the support points of a FE_Q() finite element of the
   * same degree as the degree of the required components.
   *
   * If the underlying finite element is an FE_Q(1)^spacedim, then the
   * resulting @p VectorType is a finite element field representation of the
   * vertices of the Triangulation.
   *
   * The optional ComponentMask argument can be used to specify what
   * components of the FiniteElement to use to describe the geometry. If no
   * mask is specified at construction time, then a default one is used, i.e.,
   * the first spacedim components of the FiniteElement are assumed to
   * represent the geometry of the problem.
   *
   * This function is only implemented for FiniteElements where the specified
   * components are primitive.
   *
   * @author Luca Heltai, 2015
   */
  template <int dim,
            int spacedim,
            template <int, int> class DoFHandlerType,
            typename VectorType>
  void
  get_position_vector(const DoFHandlerType<dim, spacedim> &dh,
                      VectorType &                         vector,
                      const ComponentMask &mask = ComponentMask());

  //@}

  /**
   * Exception
   */
  DeclExceptionMsg(ExcPointNotAvailableHere,
                   "The given point is inside a cell of a "
                   "parallel::distributed::Triangulation that is not "
                   "locally owned by this processor.");
} // namespace VectorTools


// Make sure we can use NormType with Patterns.
namespace Patterns
{
  namespace Tools
  {
    template <>
    struct Convert<VectorTools::NormType, void>
    {
      /**
       * Return the Correct pattern for NormType.
       */
      static std::unique_ptr<Patterns::PatternBase>
      to_pattern()
      {
        return std_cxx14::make_unique<Patterns::Selection>(
          "mean|L1_norm|L2_norm|Lp_norm|"
          "Linfty_norm|H1_seminorm|Hdiv_seminorm|"
          "H1_norm|W1p_seminorm|W1p_norm|"
          "W1infty_seminorm|W1infty_norm");
      }



      /**
       * Convert a NormType to a string.
       */
      static std::string
      to_string(const VectorTools::NormType &                 s,
                const std::unique_ptr<Patterns::PatternBase> &p =
                  Convert<VectorTools::NormType>::to_pattern())
      {
        std::string str;
        if (s == VectorTools::mean)
          str = "mean";
        else if (s == VectorTools::L1_norm)
          str = "L1_norm";
        else if (s == VectorTools::L2_norm)
          str = "L2_norm";
        else if (s == VectorTools::Lp_norm)
          str = "Lp_norm";
        else if (s == VectorTools::Linfty_norm)
          str = "Linfty_norm";
        else if (s == VectorTools::H1_seminorm)
          str = "H1_seminorm";
        else if (s == VectorTools::Hdiv_seminorm)
          str = "Hdiv_seminorm";
        else if (s == VectorTools::H1_norm)
          str = "H1_norm";
        else if (s == VectorTools::W1p_seminorm)
          str = "W1p_seminorm";
        else if (s == VectorTools::W1infty_seminorm)
          str = "W1infty_seminorm";
        else if (s == VectorTools::W1infty_norm)
          str = "W1infty_norm";
        else if (s == VectorTools::W1p_norm)
          str = "W1p_norm";
        else
          {
            AssertThrow(false, ExcMessage("Didn't recognize a norm type."));
          }
        AssertThrow(p->match(str), ExcInternalError());
        return str;
      }


      /**
       * Convert a string to a NormType.
       */
      static VectorTools::NormType
      to_value(const std::string &                           str,
               const std::unique_ptr<Patterns::PatternBase> &p =
                 Convert<VectorTools::NormType>::to_pattern())
      {
        VectorTools::NormType norm = VectorTools::mean;
        AssertThrow(p->match(str),
                    ExcMessage(
                      "String " + str +
                      " cannot be converted to VectorTools::NormType"));

        if (str == "mean")
          norm = VectorTools::mean;
        else if (str == "L1_norm")
          norm = VectorTools::L1_norm;
        else if (str == "L2_norm")
          norm = VectorTools::L2_norm;
        else if (str == "Lp_norm")
          norm = VectorTools::Lp_norm;
        else if (str == "Linfty_norm")
          norm = VectorTools::Linfty_norm;
        else if (str == "H1_seminorm")
          norm = VectorTools::H1_seminorm;
        else if (str == "Hdiv_seminorm")
          norm = VectorTools::Hdiv_seminorm;
        else if (str == "H1_norm")
          norm = VectorTools::H1_norm;
        else if (str == "W1p_seminorm")
          norm = VectorTools::W1p_seminorm;
        else if (str == "W1infty_seminorm")
          norm = VectorTools::W1infty_seminorm;
        else if (str == "W1infty_norm")
          norm = VectorTools::W1infty_norm;
        else if (str == "W1p_norm")
          norm = VectorTools::W1p_norm;
        else
          {
            AssertThrow(false, ExcMessage("Didn't recognize a norm type."));
          }
        return norm;
      }
    };
  } // namespace Tools
} // namespace Patterns

DEAL_II_NAMESPACE_CLOSE

#endif
