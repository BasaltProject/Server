// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.attributes;

import com.mojang.serialization.Codec;
import net.minecraft.ChatFormatting;
import net.minecraft.core.Holder;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.Identifier;

public class Attribute {
    public static final Codec<Holder<Attribute>> CODEC = Codec.STRING.xmap(
        name -> BuiltInRegistries.ATTRIBUTE.get(Identifier.parse(name)).orElseThrow(), holder -> holder.unwrapKey().orElseThrow().identifier().toString());
    public static final StreamCodec<RegistryFriendlyByteBuf, Holder<Attribute>> STREAM_CODEC = StreamCodec.of(
        (buf, holder) -> ByteBufCodecs.VAR_INT.encode(buf, BuiltInRegistries.ATTRIBUTE.getId(holder.value())),
        buf -> BuiltInRegistries.ATTRIBUTE.get(ByteBufCodecs.VAR_INT.decode(buf)).orElseThrow());

    private final double defaultValue;
    private boolean syncable;
    private final String descriptionId;
    private Sentiment sentiment = Sentiment.POSITIVE;

    protected Attribute(String descriptionId, double defaultValue) {
        this.defaultValue = defaultValue;
        this.descriptionId = descriptionId;
    }

    public double getDefaultValue() {
        return defaultValue;
    }

    public boolean isClientSyncable() {
        return syncable;
    }

    public Attribute setSyncable(boolean syncable) {
        this.syncable = syncable;
        return this;
    }

    public Attribute setSentiment(Sentiment sentiment) {
        this.sentiment = sentiment;
        return this;
    }

    public double sanitizeValue(double value) {
        return value;
    }

    public String getDescriptionId() {
        return descriptionId;
    }

    public ChatFormatting getStyle(boolean valueIncrease) {
        return sentiment.getStyle(valueIncrease);
    }

    public static enum Sentiment {
        POSITIVE, NEUTRAL, NEGATIVE;

        public ChatFormatting getStyle(boolean valueIncrease) {
            return switch (this) {
                case NEUTRAL -> ChatFormatting.GRAY;
                case POSITIVE -> valueIncrease ? ChatFormatting.BLUE : ChatFormatting.RED;
                case NEGATIVE -> valueIncrease ? ChatFormatting.RED : ChatFormatting.BLUE;
            };
        }
    }
}
