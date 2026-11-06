// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.predicates;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
public class AnyOfCondition extends CompositeLootItemCondition {
    public static final MapCodec<AnyOfCondition> MAP_CODEC = createCodec(AnyOfCondition::new);
    private AnyOfCondition(HolderSet<LootItemCondition> terms) {
        super(terms, context -> {
            for (Holder<LootItemCondition> term : terms) {
                if (term.value().test(context)) {
                    return true;
                }
            }
            return false;
        });
    }
    protected String typeId() {
        return "minecraft:any_of";
    }
    public MapCodec<AnyOfCondition> codec() {
        return MAP_CODEC;
    }
    public static Builder anyOf(LootItemCondition.Builder... terms) {
        return new Builder(terms);
    }
    public static class Builder extends CompositeLootItemCondition.Builder {
        public Builder(LootItemCondition.Builder... terms) {
            super(terms);
        }
        public Builder or(LootItemCondition.Builder term) {
            addTerm(term);
            return this;
        }
        public Builder or(Holder<LootItemCondition> term) {
            addTerm(term);
            return this;
        }
        protected LootItemCondition create(HolderSet<LootItemCondition> terms) {
            return new AnyOfCondition(terms);
        }
    }
}
