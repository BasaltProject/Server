package net.minecraft.world.phys;

import net.minecraft.core.Position;
import net.minecraft.core.Vec3i;
import org.basaltmc.bukkit.Unsupported;
public class Vec3 implements Position {
    public Vec3(double value, double value2, double value3) {
    }
    public static Vec3 atLowerCornerOf(Vec3i pos) {
        throw Unsupported.hit("net.minecraft.world.phys.Vec3.atLowerCornerOf");
    }
    public double x() {
        throw Unsupported.hit("net.minecraft.world.phys.Vec3.x");
    }
    public double y() {
        throw Unsupported.hit("net.minecraft.world.phys.Vec3.y");
    }
    public double z() {
        throw Unsupported.hit("net.minecraft.world.phys.Vec3.z");
    }
}
