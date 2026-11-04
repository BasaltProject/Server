// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.Arrays;
import java.util.Optional;
import java.util.function.IntFunction;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.ByIdMap;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.entity.player.Abilities;

public enum GameType implements StringRepresentable {
    SURVIVAL(0, "survival"), CREATIVE(1, "creative"), ADVENTURE(2, "adventure"), SPECTATOR(3, "spectator");

    public static final GameType DEFAULT_MODE = SURVIVAL;
    public static final StringRepresentable.EnumCodec<GameType> CODEC = StringRepresentable.fromEnum(GameType::values);
    private static final IntFunction<GameType> BY_ID = ByIdMap.continuous(GameType::getId, values(), ByIdMap.OutOfBoundsStrategy.ZERO);
    public static final StreamCodec<ByteBuf, GameType> STREAM_CODEC = ByteBufCodecs.idMapper(BY_ID, GameType::getId);
    public static final StreamCodec<ByteBuf, Optional<GameType>> OPTIONAL_STREAM_CODEC = ByteBufCodecs.OPTIONAL_VAR_INT.map(
        id -> id.isPresent() ? Optional.of(byId(id.getAsInt())) : Optional.empty(),
        mode -> mode.isPresent() ? java.util.OptionalInt.of(mode.get().id) : java.util.OptionalInt.empty());
    public static final Codec<GameType> LEGACY_ID_CODEC = Codec.INT.xmap(GameType::byId, GameType::getId);

    private final int id;
    private final String name;
    private final Component shortName;
    private final Component longName;

    GameType(int id, String name) {
        this.id = id;
        this.name = name;
        this.shortName = Component.translatable("selectWorld.gameMode." + name);
        this.longName = Component.translatable("gameMode." + name);
    }

    public int getId() {
        return id;
    }

    public String getName() {
        return name;
    }

    public String getSerializedName() {
        return name;
    }

    public Component getLongDisplayName() {
        return longName;
    }

    public Component getShortDisplayName() {
        return shortName;
    }

    public void updatePlayerAbilities(Abilities abilities) {
        abilities.mayfly = this == CREATIVE || this == SPECTATOR;
        abilities.instabuild = this == CREATIVE;
        abilities.invulnerable = this == CREATIVE || this == SPECTATOR;
        abilities.flying = this == SPECTATOR ? true : this == CREATIVE ? abilities.flying : false;
        abilities.mayBuild = !isBlockPlacingRestricted();
    }

    public boolean isBlockPlacingRestricted() {
        return this == ADVENTURE || this == SPECTATOR;
    }

    public boolean isCreative() {
        return this == CREATIVE;
    }

    public boolean isSurvival() {
        return this == SURVIVAL || this == ADVENTURE;
    }

    public static GameType byId(int id) {
        return BY_ID.apply(id);
    }

    public static GameType byName(String name) {
        return byName(name, SURVIVAL);
    }

    public static GameType byName(String name, GameType defaultMode) {
        GameType result = CODEC.byName(name);
        return result != null ? result : defaultMode;
    }

    public static boolean isValidId(int id) {
        return Arrays.stream(values()).anyMatch(type -> type.id == id);
    }
}
