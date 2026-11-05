// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders;
import net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProviders;
import com.mojang.serialization.Codec;
import java.util.List;
import java.util.Optional;
import java.util.function.Consumer;
import net.minecraft.core.Holder;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.entries.LootPoolEntryContainer;
import net.minecraft.world.level.storage.loot.entries.UniformContainerBase;
import net.minecraft.world.level.storage.loot.functions.FunctionUserBuilder;
import net.minecraft.world.level.storage.loot.functions.LootItemFunction;
import net.minecraft.world.level.storage.loot.predicates.ConditionUserBuilder;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProvider;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProvider;
public class LootPool implements Validatable, org.basaltmc.bukkit.loot.LootJson.Node {
    private final List<LootPoolEntryContainer> entries;
    private final Optional<Holder<LootItemCondition>> condition;
    private final Optional<Holder<LootItemFunction>> modifier;
    private final Holder<ContextIntProvider> rolls;
    private final Holder<ContextFloatProvider> bonusRolls;
    public static final Codec<LootPool> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.LootPool.class, "CODEC");
    private LootPool(List<LootPoolEntryContainer> entries, Optional<Holder<LootItemCondition>> condition, Optional<Holder<LootItemFunction>> modifier, Holder<ContextIntProvider> rolls, Holder<ContextFloatProvider> bonusRolls) {
        this.entries = entries;
        this.condition = condition;
        this.modifier = modifier;
        this.rolls = rolls;
        this.bonusRolls = bonusRolls;
    }
    public void writeJson(StringBuilder out) {
        out.append("{\"rolls\":");
        org.basaltmc.bukkit.loot.LootJson.value(out, rolls);
        out.append(",\"bonus_rolls\":");
        org.basaltmc.bukkit.loot.LootJson.value(out, bonusRolls);
        out.append(",\"entries\":");
        org.basaltmc.bukkit.loot.LootJson.nodes(out, entries);
        org.basaltmc.bukkit.loot.LootJson.optional(out, "condition", condition);
        org.basaltmc.bukkit.loot.LootJson.optional(out, "modifier", modifier);
        out.append('}');
    }
    public void addRandomItems(Consumer<ItemStack> result, LootContext context) {
        String json = "{\"pools\":[" + org.basaltmc.bukkit.loot.LootJson.of(this) + "]}";
        LootTable.rollNative(null, json, context.getParams(), context.getRandom().nextLong(), result);
    }
    public void validate(ValidationContext output) {
        return;
    }
    public static Builder lootPool() {
        return new Builder();
    }
    public static class Builder implements FunctionUserBuilder<Builder>, ConditionUserBuilder<Builder> {
        private final List<LootPoolEntryContainer.Builder<?>> entries = new java.util.ArrayList<>();
        private final List<Holder<LootItemCondition>> conditions = new java.util.ArrayList<>();
        private final List<Holder<LootItemFunction>> functions = new java.util.ArrayList<>();
        private Holder<ContextIntProvider> rolls = ContextIntProviders.exactly(1);
        private Holder<ContextFloatProvider> bonusRolls = ContextFloatProviders.exactly(0.0F);
        public Builder setRolls(Holder<ContextIntProvider> rolls) {
            this.rolls = rolls;
            return this;
        }
        public Builder unwrap() {
            return this;
        }
        public Builder setBonusRolls(Holder<ContextFloatProvider> bonusRolls) {
            this.bonusRolls = bonusRolls;
            return this;
        }
        public Builder add(LootPoolEntryContainer.Builder<?> entry) {
            entries.add(entry);
            return this;
        }
        public Builder addAll(List<? extends UniformContainerBase.Builder<?>> entries) {
            for (UniformContainerBase.Builder<?> entry : entries) {
                add(entry);
            }
            return this;
        }
        public Builder when(Holder<LootItemCondition> condition) {
            conditions.add(condition);
            return this;
        }
        public Builder apply(Holder<LootItemFunction> function) {
            functions.add(function);
            return this;
        }
        public LootPool build() {
            List<LootPoolEntryContainer> built = new java.util.ArrayList<>();
            for (LootPoolEntryContainer.Builder<?> entry : entries) {
                built.add(entry.build());
            }
            return new LootPool(built, ConditionUserBuilder.buildCondition(conditions),
                FunctionUserBuilder.buildFunction(functions), rolls, bonusRolls);
        }
    }
}
