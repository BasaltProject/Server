package org.basaltmc.bukkit.brigadier;

import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.brigadier.exceptions.SimpleCommandExceptionType;
import io.papermc.paper.command.brigadier.CommandSourceStack;
import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.serializer.plain.PlainTextComponentSerializer;
import org.bukkit.Bukkit;
import org.bukkit.Location;
import org.bukkit.command.CommandSender;
import org.bukkit.entity.Entity;
import org.bukkit.entity.Player;

public final class BasaltCommandSourceStack implements CommandSourceStack {
    private final CommandSender sender;
    private final Location location;
    private final Entity executor;

    public BasaltCommandSourceStack(CommandSender sender, Location location, Entity executor) {
        this.sender = sender;
        this.location = location;
        this.executor = executor;
    }

    public static BasaltCommandSourceStack of(CommandSender sender) {
        Entity entity = sender instanceof Entity found ? found : null;
        Location at = entity != null ? entity.getLocation() : Bukkit.getWorlds().get(0).getSpawnLocation();
        return new BasaltCommandSourceStack(sender, at, entity);
    }

    @Override
    public Location getLocation() {
        return location.clone();
    }

    @Override
    public CommandSender getSender() {
        return sender;
    }

    @Override
    public Entity getExecutor() {
        return executor;
    }

    @Override
    public Player getPlayerOrThrow() throws CommandSyntaxException {
        if (executor instanceof Player player) {
            return player;
        }
        throw new SimpleCommandExceptionType(() -> "A player is required to run this command here").create();
    }

    @Override
    public Entity getEntityOrThrow() throws CommandSyntaxException {
        if (executor != null) {
            return executor;
        }
        throw new SimpleCommandExceptionType(() -> "An entity is required to run this command here").create();
    }

    @Override
    public CommandSourceStack withLocation(Location location) {
        return new BasaltCommandSourceStack(sender, location.clone(), executor);
    }

    @Override
    public CommandSourceStack withExecutor(Entity executor) {
        return new BasaltCommandSourceStack(sender, location, executor);
    }

    static String plain(Component message) {
        return PlainTextComponentSerializer.plainText().serialize(message);
    }
}
