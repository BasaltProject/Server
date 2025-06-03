package org.basaltmc.server.world.densityfunction

import kotlin.test.Test
import kotlin.test.assertEquals

/** Golden vectors captured by running the real decompiled 26.2 `DensityFunctions` bytecode, not derived from this project's own code. */
class DensityFunctionVectorsTest {
    private val context = FunctionContext(10, -32, 200)

    @Test
    fun ap2MatchesVanilla() {
        val c1 = Constant(3.5)
        val c2 = Constant(-2.0)
        assertEquals(1.5, Ap2.create(Ap2Type.ADD, c1, c2).compute(context))
        assertEquals(-7.0, Ap2.create(Ap2Type.MUL, c1, c2).compute(context))
        assertEquals(-2.0, Ap2.create(Ap2Type.MIN, c1, c2).compute(context))
        assertEquals(3.5, Ap2.create(Ap2Type.MAX, c1, c2).compute(context))

        val add = Ap2.create(Ap2Type.ADD, c1, c2)
        assertEquals(1.5, add.minValue())
        assertEquals(1.5, add.maxValue())
        val mul = Ap2.create(Ap2Type.MUL, c1, c2)
        assertEquals(-7.0, mul.minValue())
        assertEquals(-7.0, mul.maxValue())
    }

    @Test
    fun yClampedGradientMatchesVanilla() {
        val grad = YClampedGradient(-64, 64, -1.0, 1.0)
        assertEquals(-0.5, grad.compute(FunctionContext(0, -32, 0)))
        assertEquals(0.0, grad.compute(FunctionContext(0, 0, 0)))
        assertEquals(-1.0, grad.compute(FunctionContext(0, -100, 0)))
        assertEquals(1.0, grad.compute(FunctionContext(0, 200, 0)))
        assertEquals(-1.0, grad.minValue())
        assertEquals(1.0, grad.maxValue())
    }

    @Test
    fun mappedMatchesVanilla() {
        val negative = Constant(-3.0)
        assertEquals(3.0, Mapped.create(MappedType.ABS, negative).compute(context))
        assertEquals(9.0, Mapped.create(MappedType.SQUARE, negative).compute(context))
        assertEquals(-27.0, Mapped.create(MappedType.CUBE, negative).compute(context))
        assertEquals(-1.5, Mapped.create(MappedType.HALF_NEGATIVE, negative).compute(context))
        assertEquals(-0.75, Mapped.create(MappedType.QUARTER_NEGATIVE, negative).compute(context))
        assertEquals(-0.3333333333333333, Mapped.create(MappedType.INVERT, negative).compute(context))

        val big = Constant(5.0)
        assertEquals(0.4583333333333333, Mapped.create(MappedType.SQUEEZE, big).compute(context))

        val spanning = YClampedGradient(-64, 64, -3.0, 3.0)
        val inverted = Mapped.create(MappedType.INVERT, spanning)
        assertEquals(Double.NEGATIVE_INFINITY, inverted.minValue())
        assertEquals(Double.POSITIVE_INFINITY, inverted.maxValue())
    }
}
