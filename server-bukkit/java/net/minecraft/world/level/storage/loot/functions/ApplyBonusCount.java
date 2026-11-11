// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import java.util.Set;
import net.minecraft.core.Holder;
import net.minecraft.resources.Identifier;
import net.minecraft.util.RandomSource;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.enchantment.Enchantment;
import net.minecraft.world.item.enchantment.EnchantmentHelper;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class ApplyBonusCount extends LootItemConditionalFunction {
    public static final MapCodec<ApplyBonusCount> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.ApplyBonusCount.class, "MAP_CODEC");
    private final Holder<Enchantment> enchantment;
    private final Formula formula;
    private ApplyBonusCount(Optional<Holder<LootItemCondition>> condition, Holder<Enchantment> enchantment, Formula formula) {
        super(condition);
        this.enchantment = enchantment;
        this.formula = formula;
    }
    public MapCodec<ApplyBonusCount> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public Set<ContextKey<?>> getReferencedContextParams() {
        return Set.of(LootContextParams.TOOL);
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        var tool = context.getOptional(LootContextParams.TOOL);
        if (tool != null) {
            int level = EnchantmentHelper.getItemEnchantmentLevel(enchantment, tool);
            itemStack.setCount(formula.calculateNewCount(context.getRandom(), itemStack.getCount(), level));
        }
        return itemStack;
    }
    public static LootItemConditionalFunction.Builder<?> addBonusBinomialDistributionCount(Holder<Enchantment> enchantment, float probability, int extraRounds) {
        return simpleBuilder(c -> new ApplyBonusCount(c, enchantment, new BinomialWithBonusCount(extraRounds, probability)));
    }
    public static LootItemConditionalFunction.Builder<?> addOreBonusCount(Holder<Enchantment> enchantment) {
        return simpleBuilder(c -> new ApplyBonusCount(c, enchantment, new OreDrops()));
    }
    public static LootItemConditionalFunction.Builder<?> addUniformBonusCount(Holder<Enchantment> enchantment) {
        return addUniformBonusCount(enchantment, 1);
    }
    public static LootItemConditionalFunction.Builder<?> addUniformBonusCount(Holder<Enchantment> enchantment, int bonusMultiplier) {
        return simpleBuilder(c -> new ApplyBonusCount(c, enchantment, new UniformBonusCount(bonusMultiplier)));
    }
    static record FormulaType(Identifier id, Codec<? extends Formula> codec) {
    }
    static record BinomialWithBonusCount(int extraRounds, float probability) implements Formula {
        public static final FormulaType TYPE = new FormulaType(Identifier.withDefaultNamespace("binomial_with_bonus_count"), null);
        public int calculateNewCount(RandomSource random, int count, int level) {
            int total = count;
            for (int round = 0; round < level + extraRounds; round++) {
                if (random.nextFloat() < probability) {
                    total++;
                }
            }
            return total;
        }
        public FormulaType getType() {
            return TYPE;
        }
    }
    static record UniformBonusCount(int bonusMultiplier) implements Formula {
        public static final Codec<UniformBonusCount> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.functions.ApplyBonusCount.UniformBonusCount.class, "CODEC");
        public static final FormulaType TYPE = new FormulaType(Identifier.withDefaultNamespace("uniform_bonus_count"), CODEC);
        public int calculateNewCount(RandomSource random, int count, int level) {
            return count + random.nextInt(bonusMultiplier * level + 1);
        }
        public FormulaType getType() {
            return TYPE;
        }
    }
    static record OreDrops() implements Formula {
        public static final OreDrops INSTANCE = new OreDrops();
        public static final Codec<OreDrops> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.functions.ApplyBonusCount.OreDrops.class, "CODEC");
        public static final FormulaType TYPE = new FormulaType(Identifier.withDefaultNamespace("ore_drops"), CODEC);
        public int calculateNewCount(RandomSource random, int count, int level) {
            if (level <= 0) {
                return count;
            }
            int roll = Math.max(random.nextInt(level + 2) - 1, 0);
            return count * (roll + 1);
        }
        public FormulaType getType() {
            return TYPE;
        }
    }
    interface Formula {
        int calculateNewCount(RandomSource random, int count, int level);
        FormulaType getType();
    }
}
