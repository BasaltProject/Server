// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.commands.functions.CommandFunction;
import net.minecraft.resources.Identifier;
import net.minecraft.server.ServerFunctionManager;
public class CacheableFunction {
    public static final Codec<CacheableFunction> CODEC = Identifier.CODEC.xmap(CacheableFunction::new, CacheableFunction::getId);
    private final Identifier id;
    public CacheableFunction(Identifier id) {
        this.id = id;
    }
    public Optional<CommandFunction<CommandSourceStack>> get(ServerFunctionManager manager) {
        return Optional.empty();
    }
    public Identifier getId() {
        return id;
    }
    @Override
    public boolean equals(Object other) {
        return other == this || other instanceof CacheableFunction that && id.equals(that.id);
    }
    @Override
    public int hashCode() {
        return id.hashCode();
    }
}
