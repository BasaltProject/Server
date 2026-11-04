// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import com.mojang.math.Transformation;
import com.mojang.serialization.Codec;
import java.util.List;
import java.util.function.IntFunction;
import net.minecraft.network.chat.Component;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.FormattedCharSequence;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.item.ItemDisplayContext;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.PushReaction;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.AABB;
public abstract class Display extends Entity {
    public static final int NO_BRIGHTNESS_OVERRIDE = -1;
    public static final String TAG_POS_ROT_INTERPOLATION_DURATION = "teleport_duration";
    public static final String TAG_TRANSFORMATION_INTERPOLATION_DURATION = "interpolation_duration";
    public static final String TAG_TRANSFORMATION_START_INTERPOLATION = "start_interpolation";
    public static final String TAG_TRANSFORMATION = "transformation";
    public static final String TAG_BILLBOARD = "billboard";
    public static final String TAG_BRIGHTNESS = "brightness";
    public static final String TAG_VIEW_RANGE = "view_range";
    public static final String TAG_SHADOW_RADIUS = "shadow_radius";
    public static final String TAG_SHADOW_STRENGTH = "shadow_strength";
    public static final String TAG_WIDTH = "width";
    public static final String TAG_HEIGHT = "height";
    public static final String TAG_GLOW_COLOR_OVERRIDE = "glow_color_override";
    protected boolean updateRenderState;
    public Display(EntityType<?> type, Level level) {
        super(type, level);
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    public final boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return false;
    }
    public void tick() {
    }
    protected InterpolationHandler createInterpolationHandler() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Display#createInterpolationHandler");
    }
    protected abstract void updateRenderSubState(boolean shouldInterpolate, float progress);
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    public AABB getInterpolatedBoundingBox(float partialTicks) {
        return getBoundingBox();
    }
    public boolean affectedByCulling() {
        return true;
    }
    public PushReaction getPistonPushReaction() {
        return PushReaction.IGNORE_ENTITY;
    }
    public boolean isIgnoringBlockTriggers() {
        return true;
    }
    public RenderState renderState() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Display#renderState");
    }
    public float calculateInterpolationProgress(float partialTickTime) {
        return 1.0F;
    }
    public void setPos(double x, double y, double z) {
        super.setPos(x, y, z);
    }
    public boolean shouldRenderAtSqrDistance(double distanceSqr) {
        return true;
    }
    public int getTeamColor() {
        return -1;
    }
    public static enum BillboardConstraints implements StringRepresentable {
        FIXED, VERTICAL, HORIZONTAL, CENTER;
        public static final Codec<BillboardConstraints> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.Display.BillboardConstraints.class, "CODEC");
        public static final IntFunction<BillboardConstraints> BY_ID = id -> values()[Math.floorMod(id, values().length)];
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
    public static record RenderState(GenericInterpolator<Transformation> transformation, BillboardConstraints billboardConstraints, int brightnessOverride, FloatInterpolator shadowRadius, FloatInterpolator shadowStrength, int glowColorOverride) {
    }
    public static class ItemDisplay extends Display {
        public ItemDisplay(EntityType<?> type, Level level) {
            super(type, level);
        }
        protected void defineSynchedData(SynchedEntityData.Builder entityData) {
        }
        public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
        }
        protected void readAdditionalSaveData(ValueInput input) {
        }
        protected void addAdditionalSaveData(ValueOutput output) {
        }
        public SlotAccess getSlot(int slot) {
            if (slot != 0) {
                return null;
            }
            return new SlotAccess() {
                public net.minecraft.world.item.ItemStack get() {
                    return live() instanceof org.bukkit.entity.ItemDisplay api ? org.bukkit.craftbukkit.inventory.CraftItemStack.asNMSCopy(api.getItemStack()) : net.minecraft.world.item.ItemStack.EMPTY;
                }

                public boolean set(net.minecraft.world.item.ItemStack stack) {
                    if (live() instanceof org.bukkit.entity.ItemDisplay api) {
                        api.setItemStack(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(stack));
                        return true;
                    }
                    return false;
                }
            };
        }
        public ItemRenderState itemRenderState() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Display.ItemDisplay#itemRenderState");
        }
        protected void updateRenderSubState(boolean shouldInterpolate, float progress) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Display.ItemDisplay#updateRenderSubState");
        }
        public static record ItemRenderState(ItemStack itemStack, ItemDisplayContext itemTransform) {
        }
    }
    public static class BlockDisplay extends Display {
        public static final String TAG_BLOCK_STATE = "block_state";
        public BlockDisplay(EntityType<?> type, Level level) {
            super(type, level);
        }
        protected void defineSynchedData(SynchedEntityData.Builder entityData) {
        }
        public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
        }
        protected void readAdditionalSaveData(ValueInput input) {
        }
        protected void addAdditionalSaveData(ValueOutput output) {
        }
        public BlockRenderState blockRenderState() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Display.BlockDisplay#blockRenderState");
        }
        protected void updateRenderSubState(boolean shouldInterpolate, float progress) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Display.BlockDisplay#updateRenderSubState");
        }
        public static record BlockRenderState(BlockState blockState) {
        }
    }
    public static class TextDisplay extends Display {
        public static final String TAG_TEXT = "text";
        public static final byte FLAG_SHADOW = (byte) 1;
        public static final byte FLAG_SEE_THROUGH = (byte) 2;
        public static final byte FLAG_USE_DEFAULT_BACKGROUND = (byte) 4;
        public static final byte FLAG_ALIGN_LEFT = (byte) 8;
        public static final byte FLAG_ALIGN_RIGHT = (byte) 16;
        public static final int INITIAL_BACKGROUND = 1073741824;
        public TextDisplay(EntityType<?> type, Level level) {
            super(type, level);
        }
        protected void defineSynchedData(SynchedEntityData.Builder entityData) {
        }
        public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
        }
        protected void readAdditionalSaveData(ValueInput input) {
        }
        protected void addAdditionalSaveData(ValueOutput output) {
        }
        protected void updateRenderSubState(boolean shouldInterpolate, float progress) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Display.TextDisplay#updateRenderSubState");
        }
        public TextRenderState textRenderState() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Display.TextDisplay#textRenderState");
        }
        public CachedInfo cacheDisplay(LineSplitter splitter) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.Display.TextDisplay#cacheDisplay");
        }
        public static Align getAlign(byte flags) {
            if ((flags & 8) != 0) {
                return Align.LEFT;
            }
            return (flags & 16) != 0 ? Align.RIGHT : Align.CENTER;
        }
        public static enum Align implements StringRepresentable {
            CENTER, LEFT, RIGHT;
            public static final Codec<Align> CODEC = StringRepresentable.fromEnum(Align::values);
            public String getSerializedName() {
                return name().toLowerCase(java.util.Locale.ROOT);
            }
        }
        public static record CachedLine(FormattedCharSequence contents, int width) {
        }
        public static record CachedInfo(List<CachedLine> lines, int width) {
        }
        public static record TextRenderState(Component text, int lineWidth, IntInterpolator textOpacity, IntInterpolator backgroundColor, byte flags) {
        }
        public interface LineSplitter {
            CachedInfo split(Component input, int width);
        }
    }
    public interface GenericInterpolator<T> {
        static <T> GenericInterpolator<T> constant(T value) {
            return progress -> value;
        }
        T get(float progress);
    }
    static record TransformationInterpolator(Transformation previous, Transformation current) implements GenericInterpolator<Transformation> {
        public Transformation get(float progress) {
            return previous.slerp(current, progress);
        }
    }
    public interface IntInterpolator {
        static IntInterpolator constant(int value) {
            return progress -> value;
        }
        int get(float progress);
    }
    static record LinearIntInterpolator(int previous, int current) implements IntInterpolator {
        public int get(float progress) {
            return (int) Math.floor(previous + (current - previous) * (double) progress);
        }
    }
    static record ColorInterpolator(int previous, int current) implements IntInterpolator {
        public int get(float progress) {
            int a = Math.round(((previous >>> 24) & 0xFF) + (((current >>> 24) & 0xFF) - ((previous >>> 24) & 0xFF)) * progress);
            int r = Math.round(((previous >> 16) & 0xFF) + (((current >> 16) & 0xFF) - ((previous >> 16) & 0xFF)) * progress);
            int g = Math.round(((previous >> 8) & 0xFF) + (((current >> 8) & 0xFF) - ((previous >> 8) & 0xFF)) * progress);
            int b = Math.round((previous & 0xFF) + ((current & 0xFF) - (previous & 0xFF)) * progress);
            return (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
    public interface FloatInterpolator {
        static FloatInterpolator constant(float value) {
            return progress -> value;
        }
        float get(float progress);
    }
    static record LinearFloatInterpolator(float previous, float current) implements FloatInterpolator {
        public float get(float progress) {
            return previous + (current - previous) * progress;
        }
    }
}
