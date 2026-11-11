// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments.selector;
import net.minecraft.commands.arguments.EntityArgument;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.serialization.Codec;
import java.util.List;
import java.util.UUID;
import java.util.function.BiConsumer;
import java.util.function.Function;
import java.util.function.Predicate;
import net.minecraft.advancements.predicates.MinMaxBounds;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.network.chat.Component;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.util.CompilableString;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;
public class EntitySelector {
    private String raw = "";
    private int maxResults = Integer.MAX_VALUE;
    private boolean includesEntities = true;

    /** Builds a selector that the server resolves from its text form. */
    public static EntitySelector ofText(String text, int maxResults, boolean includesEntities) {
        EntitySelector selector = new EntitySelector();
        selector.raw = text;
        selector.maxResults = maxResults;
        selector.includesEntities = includesEntities;
        return selector;
    }

    private static Entity handleOf(org.bukkit.entity.Entity entity) {
        if (entity instanceof org.bukkit.craftbukkit.entity.CraftPlayer player) {
            return player.getHandle();
        }
        return entity instanceof org.bukkit.craftbukkit.entity.CraftEntity craft ? craft.getHandle() : null;
    }

    private EntitySelector() {}

    public static final int INFINITE = 2147483647;
    public static final BiConsumer<Vec3, List<? extends Entity>> ORDER_ARBITRARY = (p, c) -> {
    };
    public static final Codec<CompilableString<EntitySelector>> COMPILABLE_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.commands.arguments.selector.EntitySelector.class, "COMPILABLE_CODEC");
    public EntitySelector(int maxResults, boolean includesEntities, boolean worldLimited, List<Predicate<Entity>> contextFreePredicates, MinMaxBounds. Doubles range, Function<Vec3, Vec3> position, AABB aabb, BiConsumer<Vec3, List<? extends Entity>> order, boolean currentEntity, String playerName, UUID entityUUID, EntityType<?> type, boolean usesSelector) {
        
    }
    public int getMaxResults() {
        return maxResults;
    }
    public boolean includesEntities() {
        return includesEntities;
    }
    public boolean isSelfSelector() {
        return raw.equals("@s");
    }
    public boolean isWorldLimited() {
        return false;
    }
    public boolean usesSelector() {
        return raw.startsWith("@");
    }
    public Entity findSingleEntity(CommandSourceStack sender) throws CommandSyntaxException {
        List<? extends Entity> found = findEntities(sender);
        if (found.size() != 1) {
            throw EntityArgument.ERROR_NOT_SINGLE_ENTITY.create();
        }
        return found.get(0);
    }
    public List<? extends Entity> findEntities(CommandSourceStack sender) throws CommandSyntaxException {
        java.util.List<Entity> result = new java.util.ArrayList<>();
        org.bukkit.command.CommandSender who = org.basaltmc.bukkit.facade.BukkitCommandBridge.senderOf(sender);
        if (usesSelector()) {
            try {
        for (org.bukkit.entity.Entity entity : org.bukkit.Bukkit.getServer().selectEntities(who, raw)) {
            if (!includesEntities && !(entity instanceof org.bukkit.entity.Player)) {
                continue;
            }
            Entity handle = handleOf(entity);
            if (handle != null) {
                result.add(handle);
            }
        }
            } catch (IllegalArgumentException e) {
        throw EntityArgument.NO_ENTITIES_FOUND.create();
            }
        } else {
            org.bukkit.entity.Player player = org.bukkit.Bukkit.getServer().getPlayerExact(raw);
            if (player == null) {
        try {
            player = org.bukkit.Bukkit.getServer().getPlayer(java.util.UUID.fromString(raw));
        } catch (IllegalArgumentException ignored) {
            player = null;
        }
            }
            if (player instanceof org.bukkit.craftbukkit.entity.CraftEntity craft) {
        result.add(craft.getHandle());
            }
        }
        if (result.size() > maxResults) {
            return result.subList(0, maxResults);
        }
        return result;
    }
    public ServerPlayer findSinglePlayer(CommandSourceStack sender) throws CommandSyntaxException {
        List<ServerPlayer> found = findPlayers(sender);
        if (found.size() != 1) {
            throw EntityArgument.ERROR_NOT_SINGLE_PLAYER.create();
        }
        return found.get(0);
    }
    public List<ServerPlayer> findPlayers(CommandSourceStack sender) throws CommandSyntaxException {
        List<ServerPlayer> players = new java.util.ArrayList<>();
        for (Entity entity : findEntities(sender)) {
            if (entity instanceof ServerPlayer player) {
        players.add(player);
            }
        }
        return players;
    }
    public static Component joinNames(List<? extends Entity> entities) {
        return net.minecraft.network.chat.ComponentUtils.formatList(entities, entity -> entity.getDisplayName());
    }
}
