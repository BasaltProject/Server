// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.predicates;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
public class AllOfCondition extends CompositeLootItemCondition {
    public static final MapCodec<AllOfCondition> MAP_CODEC = createCodec(AllOfCondition::new);
    private AllOfCondition(HolderSet<LootItemCondition> terms) {
        super(terms, context -> {
            for (Holder<LootItemCondition> term : terms) {
                if (!term.value().test(context)) {
                    return false;
                }
            }
            return true;
        });
    }
    protected String typeId() {
        return "minecraft:all_of";
    }
    public static AllOfCondition allOf(HolderSet<LootItemCondition> terms) {
        return new AllOfCondition(terms);
    }
    public MapCodec<AllOfCondition> codec() {
        return MAP_CODEC;
    }
    public static Builder allOf(LootItemCondition.Builder... terms) {
        return new Builder(terms);
    }
    public static class Builder extends CompositeLootItemCondition.Builder {
        public Builder(LootItemCondition.Builder... terms) {
            super(terms);
        }
        public Builder and(LootItemCondition.Builder term) {
            addTerm(term);
            return this;
        }
        protected LootItemCondition create(HolderSet<LootItemCondition> terms) {
            return new AllOfCondition(terms);
        }
    }
}
