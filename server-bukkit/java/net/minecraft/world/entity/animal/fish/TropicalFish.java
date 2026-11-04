// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.fish;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.List;
import java.util.function.Consumer;
import net.minecraft.core.BlockPos;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.item.DyeColor;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.TooltipFlag;
import net.minecraft.world.item.component.TooltipProvider;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class TropicalFish extends AbstractSchoolingFish {
    public static final Variant DEFAULT_VARIANT = new Variant(Pattern.KOB, DyeColor.WHITE, DyeColor.WHITE);
    public static final List<Variant> COMMON_VARIANTS = List.of(
        v(Pattern.STRIPEY, DyeColor.ORANGE, DyeColor.GRAY), v(Pattern.FLOPPER, DyeColor.GRAY, DyeColor.GRAY),
        v(Pattern.FLOPPER, DyeColor.GRAY, DyeColor.BLUE), v(Pattern.CLAYFISH, DyeColor.WHITE, DyeColor.GRAY),
        v(Pattern.SUNSTREAK, DyeColor.BLUE, DyeColor.GRAY), v(Pattern.KOB, DyeColor.ORANGE, DyeColor.WHITE),
        v(Pattern.SPOTTY, DyeColor.PINK, DyeColor.LIGHT_BLUE), v(Pattern.BLOCKFISH, DyeColor.PURPLE, DyeColor.YELLOW),
        v(Pattern.CLAYFISH, DyeColor.WHITE, DyeColor.RED), v(Pattern.SPOTTY, DyeColor.WHITE, DyeColor.YELLOW),
        v(Pattern.GLITTER, DyeColor.WHITE, DyeColor.GRAY), v(Pattern.CLAYFISH, DyeColor.WHITE, DyeColor.ORANGE),
        v(Pattern.DASHER, DyeColor.CYAN, DyeColor.PINK), v(Pattern.BRINELY, DyeColor.LIME, DyeColor.LIGHT_BLUE),
        v(Pattern.BETTY, DyeColor.RED, DyeColor.WHITE), v(Pattern.SNOOPER, DyeColor.GRAY, DyeColor.RED),
        v(Pattern.BLOCKFISH, DyeColor.RED, DyeColor.WHITE), v(Pattern.FLOPPER, DyeColor.WHITE, DyeColor.YELLOW),
        v(Pattern.KOB, DyeColor.RED, DyeColor.WHITE), v(Pattern.SUNSTREAK, DyeColor.GRAY, DyeColor.WHITE),
        v(Pattern.DASHER, DyeColor.CYAN, DyeColor.YELLOW), v(Pattern.FLOPPER, DyeColor.YELLOW, DyeColor.YELLOW));
    private static Variant v(Pattern pattern, DyeColor base, DyeColor patternColor) {
        return new Variant(pattern, base, patternColor);
    }
    public TropicalFish(EntityType<? extends TropicalFish> type, Level level) {
        super(type, level);
    }
    public static String getPredefinedName(int index) {
        return "entity.minecraft.tropical_fish.predefined." + index;
    }
    public static DyeColor getBaseColor(int packedVariant) {
        return DyeColor.byId(packedVariant >> 16 & 255);
    }
    public static DyeColor getPatternColor(int packedVariant) {
        return DyeColor.byId(packedVariant >> 24 & 255);
    }
    public static Pattern getPattern(int packedVariant) {
        return Pattern.byId(packedVariant & 65535);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public DyeColor getBaseColor() {
        return live() instanceof org.bukkit.entity.TropicalFish api ? DyeColor.byId(api.getBodyColor().ordinal()) : DyeColor.WHITE;
    }
    public DyeColor getPatternColor() {
        return live() instanceof org.bukkit.entity.TropicalFish api ? DyeColor.byId(api.getPatternColor().ordinal()) : DyeColor.WHITE;
    }
    public Pattern getPattern() {
        return Pattern.values()[live() instanceof org.bukkit.entity.TropicalFish api ? api.getPattern().ordinal() : 0];
    }
    protected void applyImplicitComponents(DataComponentGetter components) {
        }
    protected <T> boolean applyImplicitComponent(DataComponentType<T> type, T value) {
        return false;
    }
    public void saveToBucketTag(ItemStack bucket) {
        net.minecraft.world.item.component.CustomData.update(net.minecraft.core.component.DataComponents.BUCKET_ENTITY_DATA, bucket, tag -> tag.putInt("BucketVariantTag", new Variant(getPattern(), getBaseColor(), getPatternColor()).getPackedId()));
    }
    public ItemStack getBucketItemStack() {
        return new ItemStack(net.minecraft.world.item.Items.TROPICAL_FISH_BUCKET);
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.tropical_fish.ambient");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.tropical_fish.death");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.tropical_fish.hurt");
    }
    protected SoundEvent getFlopSound() {
        return null;
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public static boolean checkTropicalFishSpawnRules(EntityType<TropicalFish> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return true;
    }
    public static enum Base {
        SMALL, LARGE;
    }
    public static record Variant(Pattern pattern, DyeColor baseColor, DyeColor patternColor) {
        public static final Codec<Variant> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.fish.TropicalFish.Variant.class, "CODEC");
        public Variant(int packedId) {
            this(TropicalFish.getPattern(packedId), TropicalFish.getBaseColor(packedId), TropicalFish.getPatternColor(packedId));
        }
        public int getPackedId() {
            return pattern.getPackedId() & 65535 | (baseColor.getId() & 255) << 16 | (patternColor.getId() & 255) << 24;
        }
    }
    public static enum Pattern implements StringRepresentable, TooltipProvider {
        KOB, SUNSTREAK, SNOOPER, DASHER, BRINELY, SPOTTY, FLOPPER, STRIPEY, GLITTER, BLOCKFISH, BETTY, CLAYFISH;
        public static final Codec<Pattern> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.fish.TropicalFish.Pattern.class, "CODEC");
        public static final StreamCodec<ByteBuf, Pattern> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.entity.animal.fish.TropicalFish.Pattern.class, "STREAM_CODEC");
        public static Pattern byId(int packedId) {
            for (Pattern pattern : values()) {
                if (pattern.getPackedId() == packedId) {
                    return pattern;
                }
            }
            return KOB;
        }
        public Base base() {
        return ordinal() < 6 ? Base.SMALL : Base.LARGE;
    }
        public int getPackedId() {
            return base().ordinal() | (ordinal() % 6) << 8;
        }
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
        public Component displayName() {
        return Component.translatable("entity.minecraft.tropical_fish.type." + getSerializedName());
    }
        public void addToTooltip(Item.TooltipContext context, Consumer<Component> consumer, TooltipFlag flag, DataComponentGetter components) {
            DyeColor base = components.getOrDefault(net.minecraft.core.component.DataComponents.TROPICAL_FISH_BASE_COLOR, DEFAULT_VARIANT.baseColor());
            DyeColor patternColor = components.getOrDefault(net.minecraft.core.component.DataComponents.TROPICAL_FISH_PATTERN_COLOR, DEFAULT_VARIANT.patternColor());
            net.minecraft.ChatFormatting[] styles = {net.minecraft.ChatFormatting.ITALIC, net.minecraft.ChatFormatting.GRAY};
            int common = COMMON_VARIANTS.indexOf(new Variant(this, base, patternColor));
            if (common != -1) {
                consumer.accept(Component.translatable(getPredefinedName(common)).withStyle(styles));
                return;
            }
            consumer.accept(displayName().plainCopy().withStyle(styles));
            net.minecraft.network.chat.MutableComponent colors = Component.translatable("color.minecraft." + base.getName());
            if (base != patternColor) {
                colors.append(", ").append(Component.translatable("color.minecraft." + patternColor.getName()));
            }
            consumer.accept(colors.withStyle(styles));
        }
    }
    static class TropicalFishGroupData extends AbstractSchoolingFish.SchoolSpawnGroupData {
        private TropicalFishGroupData(TropicalFish leader, Variant variant) {
            super(leader);
        }
    }
}
