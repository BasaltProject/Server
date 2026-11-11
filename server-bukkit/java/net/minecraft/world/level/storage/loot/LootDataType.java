// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot;
import net.minecraft.core.registries.Registries;
import net.minecraft.util.ProblemReporter;
import net.minecraft.world.level.storage.loot.parameters.LootContextParamSets;
import java.util.stream.Stream;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.Registry;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.context.ContextKeySet;
import net.minecraft.world.item.slot.SlotSource;
import net.minecraft.world.level.storage.loot.functions.LootItemFunction;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProvider;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProvider;
public record LootDataType<T extends Validatable>(ResourceKey<Registry<T>> registryKey, LootDataType.ContextGetter<T> contextGetter) {
    public static final LootDataType<LootItemCondition> PREDICATE = new LootDataType<>(Registries.PREDICATE, ContextGetter.constant(LootContextParamSets.ALL_PARAMS));
    public static final LootDataType<LootItemFunction> MODIFIER = new LootDataType<>(Registries.ITEM_MODIFIER, ContextGetter.constant(LootContextParamSets.ALL_PARAMS));
    public static final LootDataType<SlotSource> SLOT_SOURCE = new LootDataType<>(Registries.SLOT_SOURCE, ContextGetter.constant(LootContextParamSets.ALL_PARAMS));
    public static final LootDataType<LootTable> TABLE = new LootDataType<LootTable>((net.minecraft.resources.ResourceKey) Registries.LOOT_TABLE, LootTable::getParamSet);
    public static final LootDataType<ContextFloatProvider> FLOAT_PROVIDER = new LootDataType<>(Registries.CONTEXT_FLOAT_PROVIDER, ContextGetter.constant(LootContextParamSets.ALL_PARAMS));
    public static final LootDataType<ContextIntProvider> INT_PROVIDER = new LootDataType<>(Registries.CONTEXT_INT_PROVIDER, ContextGetter.constant(LootContextParamSets.ALL_PARAMS));
    public void runValidation(ValidationContextSource contextSource, ResourceKey<T> key, T value) {
        ValidationContext root = contextSource.context(contextGetter.context(value)).enterElement(new ProblemReporter.RootElementPathElement(key), key);
        value.validate(root);
    }
    public void runValidation(ValidationContextSource contextSource, HolderLookup<T> lookup) {
        lookup.listElements().forEach(holder -> runValidation(contextSource, holder.key(), holder.value()));
    }
    public void runValidation(ValidationContextSource contextSource, HolderLookup.Provider registries) {
        runValidation(contextSource, registries.lookupOrThrow(registryKey()));
    }
    public void runValidationIfPresent(ValidationContextSource contextSource, HolderLookup.Provider registries) {
        registries.lookup(registryKey()).ifPresent(registry -> runValidation(contextSource, registry));
    }
    public static Stream<LootDataType<?>> values() {
        return Stream.of(PREDICATE, MODIFIER, SLOT_SOURCE, TABLE, FLOAT_PROVIDER, INT_PROVIDER);
    }
    public interface ContextGetter<T> {
        ContextKeySet context(T value);
        static <T> ContextGetter<T> constant(ContextKeySet v) {
            return value -> v;


        }
    }
}
