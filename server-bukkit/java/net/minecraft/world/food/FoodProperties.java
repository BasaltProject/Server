// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.food;
import net.minecraft.util.RandomSource;
import com.mojang.serialization.Codec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.component.Consumable;
import net.minecraft.world.item.component.ConsumableListener;
import net.minecraft.world.level.Level;
public record FoodProperties(int nutrition, float saturation, boolean canAlwaysEat) implements ConsumableListener {
    public static final Codec<FoodProperties> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.food.FoodProperties.class, "DIRECT_CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, FoodProperties> DIRECT_STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.food.FoodProperties.class, "DIRECT_STREAM_CODEC");
    public void onConsume(Level level, LivingEntity user, ItemStack stack, Consumable consumable) {
        RandomSource random = user.getRandom();
        level.playSound(null, user.getX(), user.getY(), user.getZ(), consumable.sound().value(), net.minecraft.sounds.SoundSource.NEUTRAL, 1.0F, random.triangle(1.0F, 0.4F));
        if (user instanceof net.minecraft.world.entity.player.Player player) {
            player.getFoodData().eat(this);
            level.playSound(null, player.getX(), player.getY(), player.getZ(), net.minecraft.sounds.SoundEvents.PLAYER_BURP, net.minecraft.sounds.SoundSource.PLAYERS, 0.5F, 0.9F + random.nextFloat() * 0.1F);
        }
    }
    public static class Builder {
        private int nutrition;
        private float saturationModifier;
        private boolean canAlwaysEat;
        public Builder nutrition(int nutrition) {
            this.nutrition = nutrition;
            return this;
        }
        public Builder saturationModifier(float saturationModifier) {
            this.saturationModifier = saturationModifier;
            return this;
        }
        public Builder alwaysEdible() {
            this.canAlwaysEat = true;
            return this;
        }
        public FoodProperties build() {
            return new FoodProperties(nutrition, nutrition * saturationModifier * 2.0F, canAlwaysEat);
        }
    }
}
