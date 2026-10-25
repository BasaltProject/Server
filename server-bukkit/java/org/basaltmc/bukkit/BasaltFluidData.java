package org.basaltmc.bukkit;

import io.papermc.paper.block.fluid.FluidData;
import org.bukkit.Fluid;
import org.bukkit.Location;
import org.bukkit.util.Vector;

final class BasaltFluidData implements FluidData {
    private final BasaltWorld world;
    private final int x;
    private final int y;
    private final int z;
    private final String fluid;
    private final boolean source;
    private final int level;
    private final boolean falling;

    BasaltFluidData(BasaltWorld world, int x, int y, int z, String row) {
        this.world = world;
        this.x = x;
        this.y = y;
        this.z = z;
        String[] parts = row.split("\\|");
        this.fluid = parts[0];
        this.source = parts[1].equals("1");
        this.level = Integer.parseInt(parts[2]);
        this.falling = parts[3].equals("1");
    }

    private BasaltFluidData(BasaltFluidData from) {
        this.world = from.world;
        this.x = from.x;
        this.y = from.y;
        this.z = from.z;
        this.fluid = from.fluid;
        this.source = from.source;
        this.level = from.level;
        this.falling = from.falling;
    }

    @Override
    public Fluid getFluidType() {
        if (fluid.equals("empty")) {
            return Fluid.EMPTY;
        }
        if (fluid.equals("water")) {
            return source ? Fluid.WATER : Fluid.FLOWING_WATER;
        }
        return source ? Fluid.LAVA : Fluid.FLOWING_LAVA;
    }

    @Override
    public FluidData clone() {
        return new BasaltFluidData(this);
    }

    @Override
    public Vector computeFlowDirection(Location location) {
        Blocks.flushAll();
        double[] flow = WorldEnvNatives.fluidFlow(world.getName(), location.getBlockX(), location.getBlockY(),
            location.getBlockZ());
        return flow == null ? new Vector() : new Vector(flow[0], flow[1], flow[2]);
    }

    @Override
    public int getLevel() {
        return level;
    }

    @Override
    public float computeHeight(Location location) {
        if (level == 0) {
            return 0.0F;
        }
        boolean above = !world.getFluidData(x, y + 1, z).getFluidType().equals(Fluid.EMPTY);
        return falling || above ? 1.0F : level / 9.0F;
    }

    @Override
    public boolean isSource() {
        return source;
    }
}
