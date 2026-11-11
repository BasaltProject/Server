// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import net.minecraft.core.component.DataComponents;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.item.DyeColor;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.entity.BannerPattern;
import net.minecraft.world.level.block.entity.BannerPatternLayers;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class SetBannerPatternFunction extends LootItemConditionalFunction {
    private final BannerPatternLayers patterns;
    private final boolean append;
    public static final MapCodec<SetBannerPatternFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SetBannerPatternFunction.class, "MAP_CODEC");
    private SetBannerPatternFunction(Optional<Holder<LootItemCondition>> condition, BannerPatternLayers patterns, boolean append) {
        super(condition);
        this.patterns = patterns;
        this.append = append;
    }
    protected ItemStack run(ItemStack itemStack, LootContext context) {
        if (append) {
            itemStack.update(DataComponents.BANNER_PATTERNS, BannerPatternLayers.EMPTY, patterns,
                (base, extra) -> new BannerPatternLayers.Builder().addAll(base).addAll(extra).build());
        } else {
            itemStack.set(DataComponents.BANNER_PATTERNS, patterns);
        }
        return itemStack;
    }
    public MapCodec<SetBannerPatternFunction> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public static Builder setBannerPattern(boolean append) {
        return new Builder(append);
    }
    public static class Builder extends LootItemConditionalFunction.Builder<Builder> {
        private final BannerPatternLayers.Builder layers = new BannerPatternLayers.Builder();
        private final boolean appending;
        private Builder(boolean append) {
            this.appending = append;
            
        }
        protected Builder getThis() {
            return this;
        }
        public LootItemFunction build() {
            return new SetBannerPatternFunction(getCondition(), layers.build(), appending);
        }
        public Builder addPattern(Holder<BannerPattern> pattern, DyeColor color) {
            layers.add(pattern, color);
            return this;
        }
    }
}
