// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot;
import java.util.Optional;
import java.util.function.Consumer;
import net.minecraft.core.HolderGetter;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.util.StringRepresentable;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.ItemInstance;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.slot.SlotSource;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.storage.loot.functions.LootItemFunction;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class LootContext {
    private final LootParams params;
    private final RandomSource random;
    private final HolderGetter.Provider lootDataResolver;
    private final java.util.Set<VisitedEntry<?>> visited = new java.util.LinkedHashSet<>();
    private LootContext(LootParams params, RandomSource random, HolderGetter.Provider lootDataResolver) {
        this.params = params;
        this.random = random;
        this.lootDataResolver = lootDataResolver;
    }
    public LootParams getParams() {
        return params;
    }
    public boolean hasParameter(ContextKey<?> key) {
        return params.contextMap().has(key);
    }
    public <T> T getOptional(ContextKey<T> key) {
        return params.contextMap().getOrDefault(key, null);
    }
    public void addDynamicDrops(Identifier location, Consumer<ItemStack> output) {
        params.addDynamicDrops(location, output);
    }
    public boolean hasVisitedElement(VisitedEntry<?> element) {
        return visited.contains(element);
    }
    public boolean pushVisitedElement(VisitedEntry<?> element) {
        return visited.add(element);
    }
    public void popVisitedElement(VisitedEntry<?> element) {
        visited.remove(element);
    }
    public HolderGetter.Provider getResolver() {
        return lootDataResolver;
    }
    public RandomSource getRandom() {
        return random;
    }
    public float getLuck() {
        return params.getLuck();
    }
    public ServerLevel getLevel() {
        return params.getLevel();
    }
    public static VisitedEntry<LootTable> createVisitedEntry(LootTable table) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.LootContext#createVisitedEntry");
    }
    public static VisitedEntry<LootItemCondition> createVisitedEntry(LootItemCondition predicate) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.LootContext#createVisitedEntry");
    }
    public static VisitedEntry<LootItemFunction> createVisitedEntry(LootItemFunction modifier) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.LootContext#createVisitedEntry");
    }
    public static VisitedEntry<SlotSource> createVisitedEntry(SlotSource slotSource) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.LootContext#createVisitedEntry");
    }
    public static class Builder {
        private final LootParams params;
        private RandomSource random;
        public Builder(LootParams params) {
            this.params = params;
        }
        public Builder withOptionalRandomSeed(long seed) {
            if (seed != 0L) {
                random = RandomSource.create(seed);
            }
            return this;
        }
        public Builder withOptionalRandomSource(RandomSource randomSource) {
            random = randomSource;
            return this;
        }
        public ServerLevel getLevel() {
            return params.getLevel();
        }
        public LootContext create(Optional<Identifier> randomSequenceKey) {
            RandomSource source = random != null ? random : RandomSource.create();
            return new LootContext(params, source, params.getLevel().registryAccess());
        }
    }
    public static enum EntityTarget implements StringRepresentable, LootContextArg.SimpleGetter<Entity> {
        THIS, ATTACKER, DIRECT_ATTACKER, ATTACKING_PLAYER, TARGET_ENTITY, INTERACTING_ENTITY;
        public static final StringRepresentable.EnumCodec<EntityTarget> CODEC = net.minecraft.util.StringRepresentable.fromEnum(net.minecraft.world.level.storage.loot.LootContext.EntityTarget::values);
        public ContextKey<? extends Entity> contextParam() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.LootContext.EntityTarget#contextParam");
        }
        public static EntityTarget getByName(String name) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.LootContext.EntityTarget#getByName");
        }
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
    public static enum BlockEntityTarget implements StringRepresentable, LootContextArg.SimpleGetter<BlockEntity> {
        BLOCK_ENTITY;
        public ContextKey<? extends BlockEntity> contextParam() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.LootContext.BlockEntityTarget#contextParam");
        }
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
    public static enum ItemStackTarget implements StringRepresentable, LootContextArg.SimpleGetter<ItemInstance> {
        TOOL;
        public ContextKey<? extends ItemInstance> contextParam() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.LootContext.ItemStackTarget#contextParam");
        }
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
    public static record VisitedEntry<T extends Validatable>(LootDataType<T> type, T value) {
    }
}
