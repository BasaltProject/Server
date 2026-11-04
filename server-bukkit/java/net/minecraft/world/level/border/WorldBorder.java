// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.border;

import com.mojang.serialization.Codec;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.saveddata.SavedData;
import net.minecraft.world.level.saveddata.SavedDataType;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.phys.shapes.VoxelShape;

public class WorldBorder extends SavedData {
    public static final double MAX_SIZE = 5.999997E7;
    public static final double MAX_CENTER_COORDINATE = 2.9999984E7;
    public static final Codec<WorldBorder> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.border.WorldBorder.class, "CODEC");
    public static final SavedDataType<WorldBorder> TYPE = null;
    private final List<BorderChangeListener> listeners = new ArrayList<>();
    private org.bukkit.WorldBorder backing;
    private double centerX;
    private double centerZ;
    private double damagePerBlock = 0.2;
    private double safeZone = 5.0;
    private int warningBlocks = 5;
    private int warningTime = 15;
    private int absoluteMaxSize = 29999984;
    private double fromSize = MAX_SIZE;
    private double toSize = MAX_SIZE;
    private long lerpStart;
    private long lerpDuration;
    private long gameTime;

    protected WorldBorder() {
    }

    public WorldBorder(Settings settings) {
        this.centerX = settings.centerX();
        this.centerZ = settings.centerZ();
        this.damagePerBlock = settings.damagePerBlock();
        this.safeZone = settings.safeZone();
        this.warningBlocks = settings.warningBlocks();
        this.warningTime = settings.warningTime();
        this.fromSize = settings.size();
        this.toSize = settings.lerpTime() > 0 ? settings.lerpTarget() : settings.size();
        this.lerpDuration = settings.lerpTime();
    }

    public static WorldBorder backedBy(org.bukkit.WorldBorder source) {
        WorldBorder border = new WorldBorder();
        border.backing = source;
        border.centerX = source.getCenter().getX();
        border.centerZ = source.getCenter().getZ();
        border.fromSize = source.getSize();
        border.toSize = source.getSize();
        border.damagePerBlock = source.getDamageAmount();
        border.safeZone = source.getDamageBuffer();
        border.warningBlocks = source.getWarningDistance();
        border.warningTime = source.getWarningTime();
        return border;
    }

    public boolean isWithinBounds(BlockPos pos) {
        return isWithinBounds(pos.getX(), pos.getZ());
    }

    public boolean isWithinBounds(Vec3 pos) {
        return isWithinBounds(pos.x, pos.z);
    }

    public boolean isWithinBounds(ChunkPos pos) {
        return isWithinBounds(pos.getMinBlockX(), pos.getMinBlockZ()) && isWithinBounds(pos.getMaxBlockX(), pos.getMaxBlockZ());
    }

    public boolean isWithinBounds(AABB aabb) {
        return aabb.maxX > getMinX() && aabb.minX < getMaxX() && aabb.maxZ > getMinZ() && aabb.minZ < getMaxZ();
    }

    public boolean isWithinBounds(double x, double z) {
        return isWithinBounds(x, z, 0.0);
    }

    public boolean isWithinBounds(double x, double z, double margin) {
        return x >= getMinX() - margin && x < getMaxX() + margin && z >= getMinZ() - margin && z < getMaxZ() + margin;
    }

    public BlockPos clampToBounds(BlockPos position) {
        return clampToBounds(position.getX(), position.getY(), position.getZ());
    }

    public BlockPos clampToBounds(Vec3 position) {
        return clampToBounds(position.x, position.y, position.z);
    }

    public BlockPos clampToBounds(double x, double y, double z) {
        Vec3 clamped = clampVec3ToBound(x, y, z);
        return BlockPos.containing(clamped.x, clamped.y, clamped.z);
    }

    public Vec3 clampVec3ToBound(Vec3 position) {
        return clampVec3ToBound(position.x, position.y, position.z);
    }

    public Vec3 clampVec3ToBound(double x, double y, double z) {
        return new Vec3(Math.max(getMinX(), Math.min(getMaxX() - 1.0E-5, x)), y,
            Math.max(getMinZ(), Math.min(getMaxZ() - 1.0E-5, z)));
    }

    public double getDistanceToBorder(Entity entity) {
        return getDistanceToBorder(entity.getX(), entity.getZ());
    }

    public double getDistanceToBorder(double x, double z) {
        return Math.min(Math.min(z - getMinZ(), getMaxZ() - z), Math.min(x - getMinX(), getMaxX() - x));
    }

    public boolean isInsideCloseToBorder(Entity source, AABB boundingBox) {
        double margin = Math.max(Math.max(getLerpSpeed() * warningTime, warningBlocks), 1.0);
        return getDistanceToBorder(boundingBox.minX, boundingBox.minZ) < margin
            || getDistanceToBorder(boundingBox.maxX, boundingBox.maxZ) < margin;
    }

    public BorderStatus getStatus() {
        if (getLerpTime() <= 0 || toSize == fromSize) {
            return BorderStatus.STATIONARY;
        }
        return toSize < fromSize ? BorderStatus.SHRINKING : BorderStatus.GROWING;
    }

    private double bound(double value) {
        return Math.max(-absoluteMaxSize, Math.min(absoluteMaxSize, value));
    }

    public double getMinX() {
        return getMinX(0.0F);
    }

    public double getMinX(float deltaPartialTick) {
        return bound(centerX - getSize() / 2.0);
    }

    public double getMinZ() {
        return getMinZ(0.0F);
    }

    public double getMinZ(float deltaPartialTick) {
        return bound(centerZ - getSize() / 2.0);
    }

    public double getMaxX() {
        return getMaxX(0.0F);
    }

    public double getMaxX(float deltaPartialTick) {
        return bound(centerX + getSize() / 2.0);
    }

    public double getMaxZ() {
        return getMaxZ(0.0F);
    }

    public double getMaxZ(float deltaPartialTick) {
        return bound(centerZ + getSize() / 2.0);
    }

    public double getCenterX() {
        return centerX;
    }

    public double getCenterZ() {
        return centerZ;
    }

    public void setCenter(double x, double z) {
        centerX = x;
        centerZ = z;
        if (backing != null) {
            backing.setCenter(x, z);
        }
        setDirty();
        listeners.forEach(listener -> listener.onSetCenter(this, x, z));
    }

    public double getSize() {
        if (getLerpTime() <= 0) {
            return toSize;
        }
        double progress = 1.0 - (double) getLerpTime() / lerpDuration;
        return fromSize + (toSize - fromSize) * progress;
    }

    public long getLerpTime() {
        return Math.max(0L, lerpStart + lerpDuration - gameTime);
    }

    public double getLerpTarget() {
        return toSize;
    }

    public void setSize(double size) {
        fromSize = size;
        toSize = size;
        lerpDuration = 0;
        if (backing != null) {
            backing.setSize(size);
        }
        setDirty();
        listeners.forEach(listener -> listener.onSetSize(this, size));
    }

    public void lerpSizeBetween(double from, double to, long ticks, long gameTime) {
        this.fromSize = from;
        this.toSize = to;
        this.lerpStart = gameTime;
        this.lerpDuration = ticks;
        this.gameTime = gameTime;
        if (backing != null) {
            backing.setSize(to, ticks);
        }
        setDirty();
        listeners.forEach(listener -> listener.onLerpSize(this, from, to, ticks, gameTime));
    }

    protected List<BorderChangeListener> getListeners() {
        return listeners;
    }

    public void addListener(BorderChangeListener listener) {
        listeners.add(listener);
    }

    public void removeListener(BorderChangeListener listener) {
        listeners.remove(listener);
    }

    public void setAbsoluteMaxSize(int absoluteMaxSize) {
        this.absoluteMaxSize = absoluteMaxSize;
    }

    public int getAbsoluteMaxSize() {
        return absoluteMaxSize;
    }

    public double getSafeZone() {
        return safeZone;
    }

    public void setSafeZone(double safeZone) {
        this.safeZone = safeZone;
        if (backing != null) {
            backing.setDamageBuffer(safeZone);
        }
        setDirty();
        listeners.forEach(listener -> listener.onSetSafeZone(this, safeZone));
    }

    public double getDamagePerBlock() {
        return damagePerBlock;
    }

    public void setDamagePerBlock(double damagePerBlock) {
        this.damagePerBlock = damagePerBlock;
        if (backing != null) {
            backing.setDamageAmount(damagePerBlock);
        }
        setDirty();
        listeners.forEach(listener -> listener.onSetDamagePerBlock(this, damagePerBlock));
    }

    public double getLerpSpeed() {
        return lerpDuration <= 0 ? 0.0 : Math.abs(toSize - fromSize) / lerpDuration;
    }

    public int getWarningTime() {
        return warningTime;
    }

    public void setWarningTime(int warningTime) {
        this.warningTime = warningTime;
        if (backing != null) {
            backing.setWarningTime(warningTime);
        }
        setDirty();
        listeners.forEach(listener -> listener.onSetWarningTime(this, warningTime));
    }

    public int getWarningBlocks() {
        return warningBlocks;
    }

    public void setWarningBlocks(int warningBlocks) {
        this.warningBlocks = warningBlocks;
        if (backing != null) {
            backing.setWarningDistance(warningBlocks);
        }
        setDirty();
        listeners.forEach(listener -> listener.onSetWarningBlocks(this, warningBlocks));
    }

    public void tick() {
        gameTime++;
    }

    public void applyInitialSettings(long gameTime) {
        this.gameTime = gameTime;
        if (lerpDuration > 0) {
            lerpStart = gameTime;
        }
    }

    public VoxelShape getCollisionShape() {
        return VoxelShape.EMPTY;
    }

    public static record Settings(double centerX, double centerZ, double damagePerBlock, double safeZone,
            int warningBlocks, int warningTime, double size, long lerpTime, double lerpTarget) {
        public static final Settings DEFAULT = new Settings(0.0, 0.0, 0.2, 5.0, 5, 15, 5.999997E7, 0L, 0.0);
        public static final Codec<Settings> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.border.WorldBorder.Settings.class, "CODEC");

        public Settings(WorldBorder border) {
            this(border.getCenterX(), border.getCenterZ(), border.getDamagePerBlock(), border.getSafeZone(),
                border.getWarningBlocks(), border.getWarningTime(), border.getSize(), border.getLerpTime(),
                border.getLerpTarget());
        }
    }
}
