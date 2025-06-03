package org.basaltmc.server.world.climate

import org.basaltmc.server.world.GenerationMetrics
import org.basaltmc.wire.common.Mth

/**
 * // parity: net.minecraft.world.level.biome.Climate.RTree, a bounding-volume tree over the seven
 * quantized climate axes, so the closest [ParameterPoint] is a descent rather than a full scan.
 * Built once per [ParameterList] and read-only after, so the shared instance needs no lock.
 *
 * One deviation: vanilla seeds each search from the previous result in a `ThreadLocal`, which
 * prunes slightly better but lets generation order decide *tied* targets. Seeding from the same
 * first leaf makes ties resolve identically on every run and thread; distances are unaffected.
 */
internal class RTree<T> private constructor(
    private val root: Node<T>,
    private val firstLeaf: Leaf<T>,
) {
    fun search(target: LongArray): T = root.search(target, firstLeaf).value

    abstract class Node<T>(
        val parameterSpace: Array<Parameter>,
    ) {
        abstract fun search(
            target: LongArray,
            candidate: Leaf<T>,
        ): Leaf<T>

        fun distance(target: LongArray): Long {
            if (GenerationMetrics.enabled) GenerationMetrics.countClimateComparison()
            var distance = 0L
            for (i in 0 until DIMENSIONS) {
                distance += Mth.square(parameterSpace[i].distance(target[i]))
            }
            return distance
        }
    }

    class Leaf<T>(
        parameterPoint: ParameterPoint,
        val value: T,
    ) : Node<T>(parameterPoint.parameterSpace().toTypedArray()) {
        override fun search(
            target: LongArray,
            candidate: Leaf<T>,
        ): Leaf<T> = this
    }

    class SubTree<T>(
        val children: Array<Node<T>>,
    ) : Node<T>(buildParameterSpace(children)) {
        override fun search(
            target: LongArray,
            candidate: Leaf<T>,
        ): Leaf<T> {
            var minDistance = candidate.distance(target)
            var closest = candidate
            for (child in children) {
                val childDistance = child.distance(target)
                if (minDistance > childDistance) {
                    val leaf = child.search(target, closest)
                    val leafDistance = if (child === leaf) childDistance else leaf.distance(target)
                    if (minDistance > leafDistance) {
                        minDistance = leafDistance
                        closest = leaf
                    }
                }
            }
            return closest
        }
    }

    companion object {
        const val DIMENSIONS: Int = 7
        private const val CHILDREN_PER_NODE = 6

        fun <T> create(values: List<Pair<ParameterPoint, T>>): RTree<T> {
            require(values.isNotEmpty()) { "Need at least one value to build the search tree." }
            val dimensions = values[0].first.parameterSpace().size
            check(dimensions == DIMENSIONS) { "Expecting parameter space to be $DIMENSIONS, got $dimensions" }
            val leaves = values.map { (point, value) -> Leaf(point, value) }
            return RTree(build(leaves.toMutableList()), leaves[0])
        }

        private fun <T> build(children: MutableList<Node<T>>): Node<T> {
            check(children.isNotEmpty()) { "Need at least one child to build a node" }
            if (children.size == 1) return children[0]
            if (children.size <= CHILDREN_PER_NODE) {
                children.sortWith(compareBy { node -> totalMagnitude(node) })
                return SubTree(children.toTypedArray())
            }

            var minCost = Long.MAX_VALUE
            var minDimension = -1
            var minBuckets: List<SubTree<T>> = emptyList()
            for (dimension in 0 until DIMENSIONS) {
                sortNodes(children, dimension, absolute = false)
                val buckets = bucketize(children)
                var totalCost = 0L
                for (bucket in buckets) totalCost += cost(bucket.parameterSpace)
                if (minCost > totalCost) {
                    minCost = totalCost
                    minDimension = dimension
                    minBuckets = buckets
                }
            }

            val ordered = minBuckets.toMutableList<Node<T>>()
            sortNodes(ordered, minDimension, absolute = true)
            // Each bucket is rebuilt from its own children rather than kept as-is, so the tree keeps
            // splitting until every node is within CHILDREN_PER_NODE.
            return SubTree(
                ordered
                    .map { bucket -> build((bucket as SubTree<T>).children.toMutableList()) }
                    .toTypedArray(),
            )
        }

        /** Grouped into buckets of `6^floor(log6(n - 0.01))`, matching vanilla's own sizing exactly. */
        private fun <T> bucketize(nodes: List<Node<T>>): List<SubTree<T>> {
            val buckets = mutableListOf<SubTree<T>>()
            var current = mutableListOf<Node<T>>()
            val expectedChildrenCount =
                pow6(kotlin.math.floor(kotlin.math.ln(nodes.size - 0.01) / kotlin.math.ln(6.0)).toInt())
            for (child in nodes) {
                current.add(child)
                if (current.size >= expectedChildrenCount) {
                    buckets.add(SubTree(current.toTypedArray()))
                    current = mutableListOf()
                }
            }
            if (current.isNotEmpty()) buckets.add(SubTree(current.toTypedArray()))
            return buckets
        }

        private fun pow6(exponent: Int): Int {
            var result = 1
            repeat(maxOf(exponent, 0)) { result *= CHILDREN_PER_NODE }
            return result
        }

        /** Vanilla chains a comparator per axis, starting at [dimension] and wrapping around. */
        private fun <T> sortNodes(
            children: MutableList<Node<T>>,
            dimension: Int,
            absolute: Boolean,
        ) {
            children.sortWith { a, b ->
                for (offset in 0 until DIMENSIONS) {
                    val axis = (dimension + offset) % DIMENSIONS
                    val comparison = center(a, axis, absolute).compareTo(center(b, axis, absolute))
                    if (comparison != 0) return@sortWith comparison
                }
                0
            }
        }

        private fun <T> center(
            node: Node<T>,
            axis: Int,
            absolute: Boolean,
        ): Long {
            val parameter = node.parameterSpace[axis]
            val center = (parameter.min + parameter.max) / 2L
            return if (absolute) kotlin.math.abs(center) else center
        }

        private fun <T> totalMagnitude(node: Node<T>): Long {
            var total = 0L
            for (axis in 0 until DIMENSIONS) {
                val parameter = node.parameterSpace[axis]
                total += kotlin.math.abs((parameter.min + parameter.max) / 2L)
            }
            return total
        }

        private fun cost(parameterSpace: Array<Parameter>): Long {
            var result = 0L
            for (parameter in parameterSpace) result += kotlin.math.abs(parameter.max - parameter.min)
            return result
        }

        private fun <T> buildParameterSpace(children: Array<Node<T>>): Array<Parameter> {
            require(children.isNotEmpty()) { "SubTree needs at least one child" }
            val bounds = arrayOfNulls<Parameter>(DIMENSIONS)
            for (child in children) {
                for (axis in 0 until DIMENSIONS) {
                    bounds[axis] = child.parameterSpace[axis].span(bounds[axis])
                }
            }
            return Array(DIMENSIONS) { axis -> bounds[axis] ?: Parameter(0L, 0L) }
        }
    }
}
