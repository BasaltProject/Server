// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.gamerules;

import com.mojang.serialization.Codec;
import it.unimi.dsi.fastutil.objects.Reference2ObjectMap;
import it.unimi.dsi.fastutil.objects.Reference2ObjectOpenHashMap;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.function.Predicate;
import java.util.stream.Stream;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.util.datafix.DataFixTypes;
import net.minecraft.world.level.saveddata.SavedData;
import net.minecraft.world.level.saveddata.SavedDataType;

public final class GameRuleMap extends SavedData {
    public static final Codec<GameRuleMap> CODEC = rawCodec().xmap(GameRuleMap::ofTrusted, GameRuleMap::map);
    public static final SavedDataType<GameRuleMap> TYPE = new SavedDataType<>(Identifier.withDefaultNamespace("game_rules"), () -> of(), CODEC, DataFixTypes.SAVED_DATA_GAME_RULES);
    private final Reference2ObjectMap<GameRule<?>, Object> map;

    private GameRuleMap(Reference2ObjectMap<GameRule<?>, Object> map) {
        this.map = map;
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static Codec<Map<GameRule<?>, Object>> rawCodec() {
        return (Codec) Codec.dispatchedMap(BuiltInRegistries.GAME_RULE.byNameCodec(), GameRule::valueCodec);
    }

    private static GameRuleMap ofTrusted(Map<GameRule<?>, Object> values) {
        return new GameRuleMap(new Reference2ObjectOpenHashMap<GameRule<?>, Object>(values));
    }

    public static GameRuleMap of() {
        return new GameRuleMap(new Reference2ObjectOpenHashMap<>());
    }

    public static GameRuleMap of(Stream<GameRule<?>> rules) {
        Reference2ObjectOpenHashMap<GameRule<?>, Object> values = new Reference2ObjectOpenHashMap<>();
        rules.forEach(rule -> values.put(rule, rule.defaultValue()));
        return new GameRuleMap(values);
    }

    public static GameRuleMap copyOf(GameRuleMap other) {
        return new GameRuleMap(new Reference2ObjectOpenHashMap<>(other.map));
    }

    public boolean has(GameRule<?> gameRule) {
        return map.containsKey(gameRule);
    }

    @SuppressWarnings("unchecked")
    public <T> T get(GameRule<T> gameRule) {
        return (T) map.get(gameRule);
    }

    public <T> void set(GameRule<T> gameRule, T value) {
        setDirty();
        map.put(gameRule, value);
    }

    public <T> void reset(GameRule<T> gameRule) {
        set(gameRule, gameRule.defaultValue());
    }

    @SuppressWarnings("unchecked")
    public <T> T remove(GameRule<T> gameRule) {
        setDirty();
        return (T) map.remove(gameRule);
    }

    public Set<GameRule<?>> keySet() {
        return map.keySet();
    }

    public int size() {
        return map.size();
    }

    @Override
    public String toString() {
        return map.toString();
    }

    public GameRuleMap withOther(GameRuleMap other) {
        GameRuleMap result = copyOf(this);
        result.setFromIf(other, rule -> true);
        return result;
    }

    public void setFromIf(GameRuleMap other, Predicate<GameRule<?>> predicate) {
        for (GameRule<?> rule : other.keySet()) {
            if (predicate.test(rule)) {
                copyRule(other, rule, this);
            }
        }
    }

    private static <T> void copyRule(GameRuleMap from, GameRule<T> rule, GameRuleMap to) {
        to.set(rule, Objects.requireNonNull(from.get(rule)));
    }

    private Reference2ObjectMap<GameRule<?>, Object> map() {
        return map;
    }

    @Override
    public boolean equals(Object obj) {
        return obj == this || obj instanceof GameRuleMap other && Objects.equals(map, other.map);
    }

    @Override
    public int hashCode() {
        return Objects.hash(map);
    }

    public static class Builder {
        private final Reference2ObjectMap<GameRule<?>, Object> values = new Reference2ObjectOpenHashMap<>();

        public <T> Builder set(GameRule<T> gameRule, T value) {
            values.put(gameRule, value);
            return this;
        }

        public GameRuleMap build() {
            return new GameRuleMap(values);
        }
    }
}
