// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction;
import net.minecraft.core.Direction;
import net.minecraft.world.level.levelgen.densityfunction.generator.ConstantFunction;
import net.minecraft.world.level.levelgen.densityfunction.generator.GradientFunction;
import net.minecraft.world.level.levelgen.densityfunction.op.SliceFunction;
public interface DfRewriteRule {
    DfRewriteRule INLINE_REFERENCE = function -> function instanceof DensityFunctions.HolderHolder held ? held.function().value() : function;
    DfRewriteRule SLICE_UNIFORM_AXES = new SliceUniformAxes(7);
    static DfRewriteRule sequence(DfRewriteRule... rules) {
        return function -> {
            DensityFunction current = function;
            for (DfRewriteRule rule : rules) {
                current = rule.rewrite(current);
            }
            return current;
        };
    }
    DensityFunction rewrite(DensityFunction function);
    public static record SliceUniformAxes(int parentDomainAxes) implements DfRewriteRule {
        private static boolean leaf(DensityFunction function) {
            return function instanceof ConstantFunction || function instanceof GradientFunction;
        }
        private static int sliced(DensityFunction function) {
            int axes = 0;
            while (function instanceof SliceFunction slice) {
                axes |= DensityFunction.axesFrom(slice.axis());
                function = slice.input();
            }
            return axes;
        }
        private DensityFunction removeAxes(DensityFunction function, int axes) {
            int missing = axes & ~sliced(function);
            DensityFunction out = function;
            if ((missing & DensityFunction.AXIS_X) != 0) {
                out = new SliceFunction(Direction.Axis.X, 0, out);
            }
            if ((missing & DensityFunction.AXIS_Z) != 0) {
                out = new SliceFunction(Direction.Axis.Z, 0, out);
            }
            if ((missing & DensityFunction.AXIS_Y) != 0) {
                out = new SliceFunction(Direction.Axis.Y, 0, out);
            }
            return out;
        }
        public DensityFunction rewrite(DensityFunction function) {
        if (leaf(function)) {
            return function;
        }
        int axes = function.domainAxes();
        if (parentDomainAxes == axes) {
            return function.rewriteChildren(this);
        }
        return removeAxes(function.rewriteChildren(new SliceUniformAxes(axes)), parentDomainAxes & ~axes);
        }
    }
}
