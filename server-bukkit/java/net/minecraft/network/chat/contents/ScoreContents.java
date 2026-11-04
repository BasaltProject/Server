// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat.contents;
import net.minecraft.network.chat.Component;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.datafixers.util.Either;
import com.mojang.serialization.MapCodec;
import net.minecraft.commands.arguments.selector.EntitySelector;
import net.minecraft.network.chat.ComponentContents;
import net.minecraft.network.chat.MutableComponent;
import net.minecraft.network.chat.ResolutionContext;
import net.minecraft.util.CompilableString;
public record ScoreContents(Either<CompilableString<EntitySelector>, String> name, String objective) implements ComponentContents {
    public static final MapCodec<ScoreContents> INNER_CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.mapCodec(instance -> instance.group(
        com.mojang.serialization.Codec.STRING.fieldOf("name").forGetter(contents -> contents.name().right().orElse("")),
        com.mojang.serialization.Codec.STRING.fieldOf("objective").forGetter(ScoreContents::objective)).apply(instance,
        (name, objective) -> new ScoreContents(com.mojang.datafixers.util.Either.right(name), objective)));
    public static final MapCodec<ScoreContents> MAP_CODEC = INNER_CODEC.fieldOf("score").xmap(contents -> contents, contents -> contents);
    public MapCodec<ScoreContents> codec() {
        return MAP_CODEC;
    }
    public MutableComponent resolve(ResolutionContext context, int recursionDepth) throws CommandSyntaxException {
        String holder = name.map(selector -> selector.source(), plain -> plain);
        if (name.left().isPresent()) {
            java.util.List<? extends net.minecraft.world.entity.Entity> found = name.left().get().compiled().findEntities(context.source());
            if (found.isEmpty()) {
        return Component.empty();
            }
            holder = found.get(0).getName().getString();
        }
        org.bukkit.scoreboard.Objective found = org.bukkit.Bukkit.getServer().getScoreboardManager().getMainScoreboard().getObjective(objective);
        if (found == null) {
            return Component.empty();
        }
        org.bukkit.scoreboard.Score score = found.getScore(holder);
        return score.isScoreSet() ? Component.literal(Integer.toString(score.getScore())) : Component.empty();
    }
}
