// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.triggers;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import org.basaltmc.bukkit.facade.TriggerKit;
import net.minecraft.advancements.predicates.entity.EntityPredicate;
import net.minecraft.core.registries.Registries;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import net.minecraft.world.phys.Vec3;
import com.mojang.serialization.Codec;
import java.util.List;
import java.util.Optional;
import net.minecraft.advancements.predicates.ItemPredicate;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.crafting.Recipe;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class RecipeCraftedTrigger extends SimpleCriterionTrigger<RecipeCraftedTrigger.TriggerInstance> {
    public Codec<TriggerInstance> codec() {
        return TriggerInstance.CODEC;
    }
    public void trigger(ServerPlayer player, ResourceKey<Recipe<?>> id, List<ItemStack> usedIngredients) {
        trigger(player, instance -> instance.matches(id, usedIngredients));
    }
    public static record TriggerInstance(Optional<Holder<LootItemCondition>> player, HolderSet<Recipe<?>> recipes, List<ItemPredicate> ingredients) implements SimpleCriterionTrigger.SimpleInstance {
        private boolean matches(ResourceKey<Recipe<?>> id, List<ItemStack> usedIngredients) {
            if (recipes.stream().noneMatch(holder -> holder.is(id))) {
                return false;
            }
            List<ItemStack> unused = new java.util.ArrayList<>(usedIngredients);
            for (ItemPredicate wanted : ingredients) {
                ItemStack hit = unused.stream().filter(wanted::test).findFirst().orElse(null);
                if (hit == null) {
                    return false;
                }
                unused.remove(hit);
            }
            return true;
        }
        public static final Codec<TriggerInstance> CODEC = RecordCodecBuilder.create(i -> i.group(
    TriggerKit.cond("player").forGetter(TriggerInstance::player),
    TriggerKit.recipeSet().fieldOf("recipes").forGetter(TriggerInstance::recipes),
    ItemPredicate.CODEC.listOf().optionalFieldOf("ingredients", List.of()).forGetter(TriggerInstance::ingredients)).apply(i, TriggerInstance::new));
        public static Criterion<TriggerInstance> craftedItem(HolderSet<Recipe<?>> recipeId, List<ItemPredicate.Builder> predicates) {
        return CriteriaTriggers.RECIPE_CRAFTED.createCriterion(
            new TriggerInstance(TriggerKit.none(), recipeId, predicates.stream().map(ItemPredicate.Builder::build).toList()));
        }
        public static Criterion<TriggerInstance> craftedItem(HolderSet<Recipe<?>> recipeId) {
        return CriteriaTriggers.RECIPE_CRAFTED.createCriterion(new TriggerInstance(TriggerKit.none(), recipeId, List.of()));
        }
        public static Criterion<TriggerInstance> crafterCraftedItem(HolderSet<Recipe<?>> recipeId) {
        return CriteriaTriggers.CRAFTER_RECIPE_CRAFTED.createCriterion(new TriggerInstance(TriggerKit.none(), recipeId, List.of()));
        }
    }
}
