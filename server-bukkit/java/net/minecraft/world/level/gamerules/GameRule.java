// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.gamerules;

import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import java.util.Objects;
import java.util.function.ToIntFunction;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.flag.FeatureElement;
import net.minecraft.world.flag.FeatureFlagSet;

public final class GameRule<T> implements FeatureElement {
    private final GameRuleCategory category;
    private final GameRuleType gameRuleType;
    private final ArgumentType<T> argument;
    private final GameRules.VisitorCaller<T> visitorCaller;
    private final Codec<T> valueCodec;
    private final ToIntFunction<T> commandResultFunction;
    private final T defaultValue;
    private final FeatureFlagSet requiredFeatures;

    public GameRule(GameRuleCategory category, GameRuleType gameRuleType, ArgumentType<T> argument, GameRules.VisitorCaller<T> visitorCaller, Codec<T> valueCodec, ToIntFunction<T> commandResultFunction, T defaultValue, FeatureFlagSet requiredFeatures) {
        this.category = category;
        this.gameRuleType = gameRuleType;
        this.argument = argument;
        this.visitorCaller = visitorCaller;
        this.valueCodec = valueCodec;
        this.commandResultFunction = commandResultFunction;
        this.defaultValue = defaultValue;
        this.requiredFeatures = requiredFeatures;
    }

    @Override
    public String toString() {
        return id();
    }

    public String id() {
        return getIdentifier().toShortString();
    }

    public Identifier getIdentifier() {
        return Objects.requireNonNull(BuiltInRegistries.GAME_RULE.getKey(this));
    }

    public Identifier getIdentifierWithFallback() {
        Identifier key = BuiltInRegistries.GAME_RULE.getKey(this);
        return key != null ? key : Identifier.withDefaultNamespace("unregistered_sadface");
    }

    public String getDescriptionId() {
        return getIdentifier().toLanguageKey("gamerule");
    }

    public String serialize(T value) {
        return value.toString();
    }

    public DataResult<T> deserialize(String value) {
        try {
            StringReader reader = new StringReader(value);
            T parsed = argument.parse(reader);
            if (reader.canRead()) {
                return DataResult.error(() -> "Failed to deserialize; trailing characters", parsed);
            }
            return DataResult.success(parsed);
        } catch (CommandSyntaxException e) {
            return DataResult.error(() -> "Failed to deserialize");
        }
    }

    @SuppressWarnings("unchecked")
    public Class<T> valueClass() {
        return (Class<T>) defaultValue.getClass();
    }

    public void callVisitor(GameRuleTypeVisitor visitor) {
        visitorCaller.call(visitor, this);
    }

    public int getCommandResult(T value) {
        return commandResultFunction.applyAsInt(value);
    }

    public GameRuleCategory category() {
        return category;
    }

    public GameRuleType gameRuleType() {
        return gameRuleType;
    }

    public ArgumentType<T> argument() {
        return argument;
    }

    public Codec<T> valueCodec() {
        return valueCodec;
    }

    public T defaultValue() {
        return defaultValue;
    }

    @Override
    public FeatureFlagSet requiredFeatures() {
        return requiredFeatures;
    }

    @Override
    public boolean isEnabled(FeatureFlagSet enabledFeatures) {
        return requiredFeatures.isSubsetOf(enabledFeatures);
    }
}
