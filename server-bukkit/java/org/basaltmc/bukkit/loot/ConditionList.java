// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package org.basaltmc.bukkit.loot;

import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;

/** Several conditions that must all hold, written as `all_of`. */
public final class ConditionList implements LootItemCondition, LootJson.Node {
    private final List<Holder<LootItemCondition>> terms;

    public ConditionList(List<Holder<LootItemCondition>> terms) {
        this.terms = List.copyOf(terms);
    }

    @Override
    public MapCodec<? extends LootItemCondition> codec() {
        return null;
    }

    @Override
    public boolean test(LootContext context) {
        for (Holder<LootItemCondition> term : terms) {
            if (!term.value().test(context)) {
                return false;
            }
        }
        return true;
    }

    @Override
    public void validate(ValidationContext context) {
    }

    @Override
    public void writeJson(StringBuilder out) {
        out.append("{\"type\":\"minecraft:all_of\",\"terms\":[");
        for (int i = 0; i < terms.size(); i++) {
            if (i > 0) {
                out.append(',');
            }
            LootJson.value(out, terms.get(i));
        }
        out.append("]}");
    }
}
