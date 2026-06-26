package org.basaltmc.bukkit.player;

import io.papermc.paper.world.damagesource.CombatEntry;
import io.papermc.paper.world.damagesource.CombatTracker;
import io.papermc.paper.world.damagesource.FallLocationType;
import java.util.ArrayList;
import java.util.List;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.BasaltWorld;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.damage.DamageSource;
import org.bukkit.damage.DamageType;
import org.bukkit.damage.DeathMessageType;
import org.bukkit.entity.Entity;
import org.bukkit.entity.LivingEntity;
import org.bukkit.entity.Player;

/** The damage a player has taken lately, kept by the server and read here with the game's own rules for combat. */
public final class BasaltCombatTracker implements CombatTracker {
    private static final int kStride = 8;
    private static final int kResetDamageTicks = 100;
    private static final int kResetCombatTicks = 300;

    private final Player player;
    private boolean inCombat;
    private int combatStart;
    private int combatEnd;

    public BasaltCombatTracker(Player player) {
        this.player = player;
    }

    private record Row(String type, String messageId, int direct, int cause, float damage, float fall,
            String location, long tick) {}

    private int now() {
        return (int) player.getWorld().getGameTime();
    }

    private List<Row> rows() {
        Object flat = PlayerCalls.call(player.getUniqueId(), "combat");
        List<Row> out = new ArrayList<>();
        if (flat instanceof String[] values) {
            for (int at = 0; at + kStride <= values.length; at += kStride) {
                out.add(new Row(values[at], values[at + 1], Integer.parseInt(values[at + 2]),
                    Integer.parseInt(values[at + 3]), Float.parseFloat(values[at + 4]),
                    Float.parseFloat(values[at + 5]), values[at + 6], Long.parseLong(values[at + 7])));
            }
        }
        return out;
    }

    private Entity entity(int id) {
        return id < 0 || !(player.getWorld() instanceof BasaltWorld world) ? null : world.entity(id);
    }

    private static boolean isFall(String type) {
        return type.equals("minecraft:fall") || type.equals("minecraft:ender_pearl")
            || type.equals("minecraft:stalagmite");
    }

    private static boolean alwaysFall(String type) {
        return type.equals("minecraft:outside_border") || type.equals("minecraft:out_of_world");
    }

    private static FallLocationType locationOf(String id) {
        return switch (id) {
            case "ladder" -> FallLocationType.LADDER;
            case "vines" -> FallLocationType.VINES;
            case "weeping_vines" -> FallLocationType.WEEPING_VINES;
            case "twisting_vines" -> FallLocationType.TWISTING_VINES;
            case "scaffolding" -> FallLocationType.SCAFFOLDING;
            case "other_climbable" -> FallLocationType.OTHER_CLIMBABLE;
            case "water" -> FallLocationType.WATER;
            case "generic" -> FallLocationType.GENERIC;
            default -> null;
        };
    }

    private DamageSource sourceOf(Row row) {
        DamageType type = Registry.DAMAGE_TYPE.get(NamespacedKey.fromString(row.type));
        if (type == null) {
            type = DamageType.GENERIC;
        }
        DamageSource.Builder builder = DamageSource.builder(type);
        Entity direct = entity(row.direct);
        Entity cause = entity(row.cause);
        if (direct != null) {
            builder.withDirectEntity(direct);
        }
        if (cause != null) {
            builder.withCausingEntity(cause);
        }
        return builder.build();
    }

    private CombatEntry entryOf(Row row) {
        return CombatEntry.combatEntry(sourceOf(row), row.damage, locationOf(row.location), row.fall);
    }

    private void recheck() {
        List<Row> rows = rows();
        if (rows.isEmpty()) {
            return;
        }
        long last = rows.get(rows.size() - 1).tick;
        int reset = inCombat ? kResetCombatTicks : kResetDamageTicks;
        if (!player.isValid() || now() - last > reset) {
            inCombat = false;
            combatEnd = now();
            PlayerCalls.call(player.getUniqueId(), "combat_clear");
            return;
        }
        if (!inCombat) {
            for (Row row : rows) {
                if (entity(row.cause) instanceof LivingEntity) {
                    inCombat = true;
                    combatStart = (int) row.tick;
                    combatEnd = combatStart;
                    break;
                }
            }
        }
    }

    @Override
    public LivingEntity getEntity() {
        return player;
    }

    @Override
    public List<CombatEntry> getCombatEntries() {
        recheck();
        List<CombatEntry> out = new ArrayList<>();
        for (Row row : rows()) {
            out.add(entryOf(row));
        }
        return List.copyOf(out);
    }

    @Override
    public void setCombatEntries(List<CombatEntry> entries) {
        PlayerCalls.call(player.getUniqueId(), "combat_clear");
        for (CombatEntry entry : entries) {
            addCombatEntry(entry);
        }
    }

    @Override
    public void addCombatEntry(CombatEntry entry) {
        DamageSource source = entry.getDamageSource();
        String type = source.getDamageType().getKey().toString();
        String message = source.getDamageType().getTranslationKey();
        FallLocationType location = entry.getFallLocationType();
        PlayerCalls.call(player.getUniqueId(), "combat_push", type,
            message.startsWith("death.attack.") ? message.substring("death.attack.".length()) : message,
            (long) (source.getDirectEntity() == null ? -1 : source.getDirectEntity().getEntityId()),
            (long) (source.getCausingEntity() == null ? -1 : source.getCausingEntity().getEntityId()),
            (double) entry.getDamage(), (double) entry.getFallDistance(), location == null ? "" : location.id(),
            (long) now());
    }

    @Override
    public CombatEntry computeMostSignificantFall() {
        Row row = mostSignificantFall(rows());
        return row == null ? null : entryOf(row);
    }

    private static Row mostSignificantFall(List<Row> rows) {
        Row result = null;
        Row alternative = null;
        float altDamage = 0.0F;
        float bestFall = 0.0F;
        for (int at = 0; at < rows.size(); at++) {
            Row row = rows.get(at);
            Row previous = at > 0 ? rows.get(at - 1) : null;
            boolean fake = alwaysFall(row.type);
            float fall = fake ? Float.MAX_VALUE : row.fall;
            if ((isFall(row.type) || fake) && fall > 0.0F && (result == null || fall > bestFall)) {
                result = at > 0 ? previous : row;
                bestFall = fall;
            }
            if (!row.location.isEmpty() && (alternative == null || row.damage > altDamage)) {
                alternative = row;
                altDamage = row.damage;
            }
        }
        if (bestFall > 5.0F && result != null) {
            return result;
        }
        return altDamage > 5.0F && alternative != null ? alternative : null;
    }

    @Override
    public boolean isInCombat() {
        recheck();
        return inCombat;
    }

    @Override
    public boolean isTakingDamage() {
        recheck();
        return !rows().isEmpty();
    }

    @Override
    public int getCombatDuration() {
        recheck();
        return inCombat ? now() - combatStart : combatEnd - combatStart;
    }

    @Override
    public int getLastDamageTime() {
        List<Row> rows = rows();
        return rows.isEmpty() ? 0 : (int) rows.get(rows.size() - 1).tick;
    }

    @Override
    public void resetCombatState() {
        inCombat = false;
        combatEnd = now();
        PlayerCalls.call(player.getUniqueId(), "combat_clear");
    }

    @Override
    public FallLocationType calculateFallLocationType() {
        List<Row> rows = rows();
        for (int at = rows.size() - 1; at >= 0; at--) {
            FallLocationType found = locationOf(rows.get(at).location);
            if (found != null) {
                return found;
            }
        }
        return null;
    }

    private Component name(Entity entity) {
        return entity == null ? null : entity.name();
    }

    private Component assisted(Entity attacker, Component attackerName, String withItem, String without) {
        org.bukkit.inventory.ItemStack held = attacker instanceof LivingEntity living && living.getEquipment() != null
            ? living.getEquipment().getItemInMainHand() : null;
        if (held != null && !held.getType().isAir() && held.hasItemMeta() && held.getItemMeta().hasDisplayName()) {
            return Component.translatable(withItem, player.name(), attackerName, held.displayName());
        }
        return Component.translatable(without, player.name(), attackerName);
    }

    @Override
    public Component getDeathMessage() {
        List<Row> rows = rows();
        if (rows.isEmpty()) {
            return Component.translatable("death.attack.generic", player.name());
        }
        Row killing = rows.get(rows.size() - 1);
        DamageSource source = sourceOf(killing);
        Row knockOff = mostSignificantFall(rows);
        DeathMessageType kind = source.getDamageType().getDeathMessageType();
        if (kind == DeathMessageType.FALL_VARIANTS && knockOff != null) {
            if (isFall(knockOff.type) || alwaysFall(knockOff.type)) {
                FallLocationType location = locationOf(knockOff.location);
                return Component.translatable("death.fell.accident." + (location == null ? "generic" : location.id()),
                    player.name());
            }
            Entity killer = source.getCausingEntity();
            Entity attacker = entity(knockOff.cause);
            Component killerName = name(killer);
            Component attackerName = name(attacker);
            if (attackerName != null && !attackerName.equals(killerName)) {
                return assisted(attacker, attackerName, "death.fell.assist.item", "death.fell.assist");
            }
            return killerName != null ? assisted(killer, killerName, "death.fell.finish.item", "death.fell.finish")
                : Component.translatable("death.fell.killer", player.name());
        }
        String id = "death.attack." + killing.messageId;
        if (kind == DeathMessageType.INTENTIONAL_GAME_DESIGN) {
            Component link = Component.translatable(id + ".link").decorate(net.kyori.adventure.text.format.TextDecoration.BOLD);
            return Component.translatable(id + ".message", player.name(),
                Component.text("[").append(link).append(Component.text("]")));
        }
        Entity causing = source.getCausingEntity();
        Entity direct = source.getDirectEntity();
        if (causing == null && direct == null) {
            return Component.translatable(id, player.name());
        }
        Entity named = causing != null ? causing : direct;
        org.bukkit.inventory.ItemStack held = causing instanceof LivingEntity living && living.getEquipment() != null
            ? living.getEquipment().getItemInMainHand() : null;
        if (held != null && !held.getType().isAir() && held.hasItemMeta() && held.getItemMeta().hasDisplayName()) {
            return Component.translatable(id + ".item", player.name(), named.name(), held.displayName());
        }
        return Component.translatable(id, player.name(), named.name());
    }
}
