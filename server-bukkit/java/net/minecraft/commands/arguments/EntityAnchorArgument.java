// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;

import com.mojang.brigadier.LiteralMessage;
import com.mojang.brigadier.StringReader;
import com.mojang.brigadier.arguments.ArgumentType;
import com.mojang.brigadier.context.CommandContext;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.DynamicCommandExceptionType;
import com.mojang.brigadier.suggestion.Suggestions;
import com.mojang.brigadier.suggestion.SuggestionsBuilder;
import java.util.Collection;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.CompletableFuture;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.phys.Vec3;

public class EntityAnchorArgument implements ArgumentType<EntityAnchorArgument.Anchor> {
    private static final DynamicCommandExceptionType ERROR_INVALID = new DynamicCommandExceptionType(
        name -> new LiteralMessage("Invalid entity anchor position " + name));

    public static Anchor getAnchor(CommandContext<CommandSourceStack> context, String name) {
        return context.getArgument(name, Anchor.class);
    }

    public static EntityAnchorArgument anchor() {
        return new EntityAnchorArgument();
    }

    public Anchor parse(StringReader reader) throws CommandSyntaxException {
        int start = reader.getCursor();
        String name = reader.readUnquotedString();
        Anchor anchor = Anchor.getByName(name);
        if (anchor == null) {
            reader.setCursor(start);
            throw ERROR_INVALID.createWithContext(reader, name);
        }
        return anchor;
    }

    public <S> CompletableFuture<Suggestions> listSuggestions(CommandContext<S> context, SuggestionsBuilder builder) {
        for (Anchor anchor : Anchor.values()) {
            if (anchor.name.startsWith(builder.getRemaining())) {
                builder.suggest(anchor.name);
            }
        }
        return builder.buildFuture();
    }

    public Collection<String> getExamples() {
        return List.of("eyes", "feet");
    }

    public static enum Anchor {
        FEET("feet"), EYES("eyes");

        private final String name;

        Anchor(String name) {
            this.name = name;
        }

        public static Anchor getByName(String name) {
            for (Anchor anchor : values()) {
                if (anchor.name.equals(name.toLowerCase(Locale.ROOT))) {
                    return anchor;
                }
            }
            return null;
        }

        public Vec3 apply(Entity entity) {
            return this == FEET ? entity.position() : entity.getEyePosition();
        }

        public Vec3 apply(CommandSourceStack source) {
            Entity entity = source.getEntity();
            return entity == null ? source.getPosition() : apply(entity);
        }
    }
}
