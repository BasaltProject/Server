// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.phys;

import java.util.List;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import org.joml.Vector3f;
import org.joml.Vector3fc;

public class AABB {
    private static final double kEpsilon = 1.0E-7;

    public final double minX;
    public final double minY;
    public final double minZ;
    public final double maxX;
    public final double maxY;
    public final double maxZ;

    public AABB(double minX, double minY, double minZ, double maxX, double maxY, double maxZ) {
        this.minX = Math.min(minX, maxX);
        this.minY = Math.min(minY, maxY);
        this.minZ = Math.min(minZ, maxZ);
        this.maxX = Math.max(minX, maxX);
        this.maxY = Math.max(minY, maxY);
        this.maxZ = Math.max(minZ, maxZ);
    }

    public AABB(BlockPos pos) {
        this(pos.getX(), pos.getY(), pos.getZ(), pos.getX() + 1, pos.getY() + 1, pos.getZ() + 1);
    }

    public AABB(Vec3 begin, Vec3 end) {
        this(begin.x, begin.y, begin.z, end.x, end.y, end.z);
    }

    public static AABB of(BoundingBox box) {
        return new AABB(box.minX(), box.minY(), box.minZ(), box.maxX() + 1, box.maxY() + 1, box.maxZ() + 1);
    }

    public static AABB unitCubeFromLowerCorner(Vec3 pos) {
        return new AABB(pos.x, pos.y, pos.z, pos.x + 1.0, pos.y + 1.0, pos.z + 1.0);
    }

    public static AABB encapsulatingFullBlocks(BlockPos pos0, BlockPos pos1) {
        return new AABB(Math.min(pos0.getX(), pos1.getX()), Math.min(pos0.getY(), pos1.getY()),
            Math.min(pos0.getZ(), pos1.getZ()), Math.max(pos0.getX(), pos1.getX()) + 1,
            Math.max(pos0.getY(), pos1.getY()) + 1, Math.max(pos0.getZ(), pos1.getZ()) + 1);
    }

    public AABB setMinX(double minX) {
        return new AABB(minX, this.minY, this.minZ, this.maxX, this.maxY, this.maxZ);
    }

    public AABB setMinY(double minY) {
        return new AABB(this.minX, minY, this.minZ, this.maxX, this.maxY, this.maxZ);
    }

    public AABB setMinZ(double minZ) {
        return new AABB(this.minX, this.minY, minZ, this.maxX, this.maxY, this.maxZ);
    }

    public AABB setMaxX(double maxX) {
        return new AABB(this.minX, this.minY, this.minZ, maxX, this.maxY, this.maxZ);
    }

    public AABB setMaxY(double maxY) {
        return new AABB(this.minX, this.minY, this.minZ, this.maxX, maxY, this.maxZ);
    }

    public AABB setMaxZ(double maxZ) {
        return new AABB(this.minX, this.minY, this.minZ, this.maxX, this.maxY, maxZ);
    }

    public double min(Direction.Axis axis) {
        return axis.choose(this.minX, this.minY, this.minZ);
    }

    public double max(Direction.Axis axis) {
        return axis.choose(this.maxX, this.maxY, this.maxZ);
    }

    @Override
    public boolean equals(Object o) {
        return this == o || o instanceof AABB other && Double.compare(other.minX, this.minX) == 0
            && Double.compare(other.minY, this.minY) == 0 && Double.compare(other.minZ, this.minZ) == 0
            && Double.compare(other.maxX, this.maxX) == 0 && Double.compare(other.maxY, this.maxY) == 0
            && Double.compare(other.maxZ, this.maxZ) == 0;
    }

    @Override
    public int hashCode() {
        int result = Long.hashCode(Double.doubleToLongBits(this.minX));
        result = 31 * result + Long.hashCode(Double.doubleToLongBits(this.minY));
        result = 31 * result + Long.hashCode(Double.doubleToLongBits(this.minZ));
        result = 31 * result + Long.hashCode(Double.doubleToLongBits(this.maxX));
        result = 31 * result + Long.hashCode(Double.doubleToLongBits(this.maxY));
        return 31 * result + Long.hashCode(Double.doubleToLongBits(this.maxZ));
    }

    public AABB contract(double xa, double ya, double za) {
        double x0 = this.minX;
        double y0 = this.minY;
        double z0 = this.minZ;
        double x1 = this.maxX;
        double y1 = this.maxY;
        double z1 = this.maxZ;
        if (xa < 0.0) {
            x0 -= xa;
        } else if (xa > 0.0) {
            x1 -= xa;
        }
        if (ya < 0.0) {
            y0 -= ya;
        } else if (ya > 0.0) {
            y1 -= ya;
        }
        if (za < 0.0) {
            z0 -= za;
        } else if (za > 0.0) {
            z1 -= za;
        }
        return new AABB(x0, y0, z0, x1, y1, z1);
    }

    public AABB expandTowards(Vec3 delta) {
        return this.expandTowards(delta.x, delta.y, delta.z);
    }

    public AABB expandTowards(double xa, double ya, double za) {
        double x0 = this.minX;
        double y0 = this.minY;
        double z0 = this.minZ;
        double x1 = this.maxX;
        double y1 = this.maxY;
        double z1 = this.maxZ;
        if (xa < 0.0) {
            x0 += xa;
        } else if (xa > 0.0) {
            x1 += xa;
        }
        if (ya < 0.0) {
            y0 += ya;
        } else if (ya > 0.0) {
            y1 += ya;
        }
        if (za < 0.0) {
            z0 += za;
        } else if (za > 0.0) {
            z1 += za;
        }
        return new AABB(x0, y0, z0, x1, y1, z1);
    }

    public AABB inflate(double xAdd, double yAdd, double zAdd) {
        return new AABB(this.minX - xAdd, this.minY - yAdd, this.minZ - zAdd, this.maxX + xAdd, this.maxY + yAdd,
            this.maxZ + zAdd);
    }

    public AABB inflate(double amountToAddInAllDirections) {
        return this.inflate(amountToAddInAllDirections, amountToAddInAllDirections, amountToAddInAllDirections);
    }

    public AABB intersect(AABB other) {
        return new AABB(Math.max(this.minX, other.minX), Math.max(this.minY, other.minY), Math.max(this.minZ, other.minZ),
            Math.min(this.maxX, other.maxX), Math.min(this.maxY, other.maxY), Math.min(this.maxZ, other.maxZ));
    }

    public AABB minmax(AABB other) {
        return new AABB(Math.min(this.minX, other.minX), Math.min(this.minY, other.minY), Math.min(this.minZ, other.minZ),
            Math.max(this.maxX, other.maxX), Math.max(this.maxY, other.maxY), Math.max(this.maxZ, other.maxZ));
    }

    public AABB move(double xa, double ya, double za) {
        return new AABB(this.minX + xa, this.minY + ya, this.minZ + za, this.maxX + xa, this.maxY + ya, this.maxZ + za);
    }

    public AABB move(BlockPos pos) {
        return this.move(pos.getX(), pos.getY(), pos.getZ());
    }

    public AABB move(Vec3 pos) {
        return this.move(pos.x, pos.y, pos.z);
    }

    public AABB move(Vector3f pos) {
        return this.move(pos.x, pos.y, pos.z);
    }

    public boolean intersects(AABB aabb) {
        return this.intersects(aabb.minX, aabb.minY, aabb.minZ, aabb.maxX, aabb.maxY, aabb.maxZ);
    }

    public boolean intersects(double minX, double minY, double minZ, double maxX, double maxY, double maxZ) {
        return this.minX < maxX && this.maxX > minX && this.minY < maxY && this.maxY > minY && this.minZ < maxZ
            && this.maxZ > minZ;
    }

    public boolean intersects(Vec3 min, Vec3 max) {
        return this.intersects(Math.min(min.x, max.x), Math.min(min.y, max.y), Math.min(min.z, max.z),
            Math.max(min.x, max.x), Math.max(min.y, max.y), Math.max(min.z, max.z));
    }

    public boolean intersects(BlockPos pos) {
        return this.intersects(pos.getX(), pos.getY(), pos.getZ(), pos.getX() + 1, pos.getY() + 1, pos.getZ() + 1);
    }

    public boolean contains(Vec3 vec) {
        return this.contains(vec.x, vec.y, vec.z);
    }

    public boolean contains(double x, double y, double z) {
        return x >= this.minX && x < this.maxX && y >= this.minY && y < this.maxY && z >= this.minZ && z < this.maxZ;
    }

    public double getSize() {
        return (this.getXsize() + this.getYsize() + this.getZsize()) / 3.0;
    }

    public double getXsize() {
        return this.maxX - this.minX;
    }

    public double getYsize() {
        return this.maxY - this.minY;
    }

    public double getZsize() {
        return this.maxZ - this.minZ;
    }

    public AABB deflate(double xSubstract, double ySubtract, double zSubtract) {
        return this.inflate(-xSubstract, -ySubtract, -zSubtract);
    }

    public AABB deflate(double amount) {
        return this.inflate(-amount);
    }

    public Optional<Vec3> clip(Vec3 from, Vec3 to) {
        return clip(this.minX, this.minY, this.minZ, this.maxX, this.maxY, this.maxZ, from, to);
    }

    public static Optional<Vec3> clip(double minX, double minY, double minZ, double maxX, double maxY, double maxZ,
        Vec3 from, Vec3 to) {
        double[] scale = {1.0};
        double dx = to.x - from.x;
        double dy = to.y - from.y;
        double dz = to.z - from.z;
        Direction hit = faceHit(minX, minY, minZ, maxX, maxY, maxZ, from, scale, null, dx, dy, dz);
        return hit == null ? Optional.empty() : Optional.of(from.add(scale[0] * dx, scale[0] * dy, scale[0] * dz));
    }

    public static BlockHitResult clip(Iterable<AABB> aabBs, Vec3 from, Vec3 to, BlockPos pos) {
        double[] scale = {1.0};
        Direction hit = null;
        double dx = to.x - from.x;
        double dy = to.y - from.y;
        double dz = to.z - from.z;
        for (AABB box : aabBs) {
            AABB moved = box.move(pos);
            hit = faceHit(moved.minX, moved.minY, moved.minZ, moved.maxX, moved.maxY, moved.maxZ, from, scale, hit, dx,
                dy, dz);
        }
        if (hit == null) {
            return null;
        }
        return new BlockHitResult(from.add(scale[0] * dx, scale[0] * dy, scale[0] * dz), hit, pos, false);
    }

    private static Direction faceHit(double minX, double minY, double minZ, double maxX, double maxY, double maxZ,
        Vec3 from, double[] scale, Direction hit, double dx, double dy, double dz) {
        Direction result = hit;
        if (dx > kEpsilon) {
            result = clipPoint(scale, result, dx, dy, dz, minX, minY, maxY, minZ, maxZ, Direction.WEST, from.x, from.y, from.z);
        } else if (dx < -kEpsilon) {
            result = clipPoint(scale, result, dx, dy, dz, maxX, minY, maxY, minZ, maxZ, Direction.EAST, from.x, from.y, from.z);
        }
        if (dy > kEpsilon) {
            result = clipPoint(scale, result, dy, dz, dx, minY, minZ, maxZ, minX, maxX, Direction.DOWN, from.y, from.z, from.x);
        } else if (dy < -kEpsilon) {
            result = clipPoint(scale, result, dy, dz, dx, maxY, minZ, maxZ, minX, maxX, Direction.UP, from.y, from.z, from.x);
        }
        if (dz > kEpsilon) {
            result = clipPoint(scale, result, dz, dx, dy, minZ, minX, maxX, minY, maxY, Direction.NORTH, from.z, from.x, from.y);
        } else if (dz < -kEpsilon) {
            result = clipPoint(scale, result, dz, dx, dy, maxZ, minX, maxX, minY, maxY, Direction.SOUTH, from.z, from.x, from.y);
        }
        return result;
    }

    private static Direction clipPoint(double[] scale, Direction current, double da, double db, double dc, double plane,
        double minB, double maxB, double minC, double maxC, Direction face, double fromA, double fromB, double fromC) {
        double s = (plane - fromA) / da;
        double pb = fromB + s * db;
        double pc = fromC + s * dc;
        if (0.0 < s && s < scale[0] && minB - kEpsilon < pb && pb < maxB + kEpsilon && minC - kEpsilon < pc
            && pc < maxC + kEpsilon) {
            scale[0] = s;
            return face;
        }
        return current;
    }

    public boolean collidedAlongVector(Vec3 vector, List<AABB> aabbs) {
        Vec3 from = this.getCenter();
        Vec3 to = from.add(vector);
        for (AABB part : aabbs) {
            AABB inflated = part.inflate(this.getXsize() * 0.5 - kEpsilon, this.getYsize() * 0.5 - kEpsilon,
                this.getZsize() * 0.5 - kEpsilon);
            if (inflated.contains(to) || inflated.contains(from) || inflated.clip(from, to).isPresent()) {
                return true;
            }
        }
        return false;
    }

    public double distanceToSqr(Vec3 point) {
        double dx = Math.max(Math.max(this.minX - point.x, point.x - this.maxX), 0.0);
        double dy = Math.max(Math.max(this.minY - point.y, point.y - this.maxY), 0.0);
        double dz = Math.max(Math.max(this.minZ - point.z, point.z - this.maxZ), 0.0);
        return net.minecraft.util.Mth.lengthSquared(dx, dy, dz);
    }

    public double distanceToSqr(AABB boundingBox) {
        double dx = Math.max(Math.max(this.minX - boundingBox.maxX, boundingBox.minX - this.maxX), 0.0);
        double dy = Math.max(Math.max(this.minY - boundingBox.maxY, boundingBox.minY - this.maxY), 0.0);
        double dz = Math.max(Math.max(this.minZ - boundingBox.maxZ, boundingBox.minZ - this.maxZ), 0.0);
        return net.minecraft.util.Mth.lengthSquared(dx, dy, dz);
    }

    @Override
    public String toString() {
        return "AABB[" + this.minX + ", " + this.minY + ", " + this.minZ + "] -> [" + this.maxX + ", " + this.maxY + ", "
            + this.maxZ + "]";
    }

    public boolean hasNaN() {
        return Double.isNaN(this.minX) || Double.isNaN(this.minY) || Double.isNaN(this.minZ) || Double.isNaN(this.maxX)
            || Double.isNaN(this.maxY) || Double.isNaN(this.maxZ);
    }

    public Vec3 getCenter() {
        return new Vec3(net.minecraft.util.Mth.lerp(0.5, this.minX, this.maxX), net.minecraft.util.Mth.lerp(0.5, this.minY, this.maxY),
            net.minecraft.util.Mth.lerp(0.5, this.minZ, this.maxZ));
    }

    public Vec3 getBottomCenter() {
        return new Vec3(net.minecraft.util.Mth.lerp(0.5, this.minX, this.maxX), this.minY, net.minecraft.util.Mth.lerp(0.5, this.minZ, this.maxZ));
    }

    public Vec3 getMinPosition() {
        return new Vec3(this.minX, this.minY, this.minZ);
    }

    public Vec3 getMaxPosition() {
        return new Vec3(this.maxX, this.maxY, this.maxZ);
    }

    public static AABB ofSize(Vec3 center, double sizeX, double sizeY, double sizeZ) {
        return new AABB(center.x - sizeX / 2.0, center.y - sizeY / 2.0, center.z - sizeZ / 2.0, center.x + sizeX / 2.0,
            center.y + sizeY / 2.0, center.z + sizeZ / 2.0);
    }

    public AABB nextDeflated() {
        return new AABB(Math.nextUp(this.minX), Math.nextUp(this.minY), Math.nextUp(this.minZ), Math.nextDown(this.maxX),
            Math.nextDown(this.maxY), Math.nextDown(this.maxZ));
    }

    public static class Builder {
        private float minX = Float.POSITIVE_INFINITY;
        private float minY = Float.POSITIVE_INFINITY;
        private float minZ = Float.POSITIVE_INFINITY;
        private float maxX = Float.NEGATIVE_INFINITY;
        private float maxY = Float.NEGATIVE_INFINITY;
        private float maxZ = Float.NEGATIVE_INFINITY;
        private boolean defined;

        public void include(Vector3fc v) {
            float x = v.x();
            float y = v.y();
            float z = v.z();
            this.minX = Math.min(x, this.minX);
            this.minY = Math.min(y, this.minY);
            this.minZ = Math.min(z, this.minZ);
            this.maxX = Math.max(x, this.maxX);
            this.maxY = Math.max(y, this.maxY);
            this.maxZ = Math.max(z, this.maxZ);
            this.defined = true;
        }

        public boolean isDefined() {
            return this.defined;
        }

        public AABB build() {
            if (!this.defined) {
                throw new IllegalStateException("Cannot build an undefined AABB. Include at least one point.");
            }
            return new AABB(this.minX, this.minY, this.minZ, this.maxX, this.maxY, this.maxZ);
        }
    }
}
