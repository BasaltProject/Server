// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates;
import com.mojang.serialization.Codec;
import java.util.List;
import net.minecraft.world.level.GameType;
public record GameTypePredicate(List<GameType> types) {
    public static final GameTypePredicate ANY = of(GameType.values());
    public static final GameTypePredicate SURVIVAL_LIKE = of(GameType.SURVIVAL, GameType.ADVENTURE);
    public static final Codec<GameTypePredicate> CODEC = GameType.CODEC.listOf().xmap(GameTypePredicate::new, GameTypePredicate::types);
    public static GameTypePredicate of(GameType... types) {
        return new GameTypePredicate(List.of(types));
    }
    public boolean matches(GameType type) {
        return types.contains(type);
    }
}
