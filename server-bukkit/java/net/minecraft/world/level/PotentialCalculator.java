// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;
import net.minecraft.core.BlockPos;
import java.util.List;
import java.util.ArrayList;
public class PotentialCalculator {
    private final List<PointCharge> charges = new java.util.ArrayList<>();
    public void addCharge(BlockPos pos, double charge) {
        if (charge != 0.0) {
            charges.add(new PointCharge(pos, charge));
        }
    }
    public double getPotentialEnergyChange(BlockPos pos, double charge) {
        if (charge == 0.0) {
            return 0.0;
        }
        double total = 0.0;
        for (PointCharge source : charges) {
            total += source.getPotentialChange(pos);
        }
        return total * charge;
    }
    static class PointCharge {
        private final BlockPos pos;
        private final double charge;
        public PointCharge(BlockPos pos, double charge) {
            this.pos = pos;
            this.charge = charge;
        }
        public double getPotentialChange(BlockPos pos) {
            double distSqr = this.pos.distSqr(pos);
            return distSqr == 0.0 ? Double.POSITIVE_INFINITY : charge / Math.sqrt(distSqr);
        }
    }
}
