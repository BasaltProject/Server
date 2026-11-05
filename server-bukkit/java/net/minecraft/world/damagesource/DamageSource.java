// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.damagesource;

import net.minecraft.core.Holder;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.phys.Vec3;
public class DamageSource {
    private final Holder<DamageType> type;
    private final Entity directEntity;
    private final Entity causingEntity;
    private final Vec3 damageSourcePosition;

    protected DamageSource() {
        this.type = null;
        this.directEntity = null;
        this.causingEntity = null;
        this.damageSourcePosition = null;
    }
    public String toString() {
        return "DamageSource (" + (type == null ? "?" : type.unwrapKey().map(key -> key.identifier().toString()).orElse("?")) + ")";
    }
    public float getFoodExhaustion() {
        return type.value().exhaustion();
    }
    public boolean isDirect() {
        return directEntity == causingEntity;
    }
    private DamageSource(Holder<DamageType> type, Entity directEntity, Entity causingEntity, Vec3 damageSourcePosition) {
        this.type = type;
        this.directEntity = directEntity;
        this.causingEntity = causingEntity;
        this.damageSourcePosition = damageSourcePosition;
    }
    public DamageSource(Holder<DamageType> type, Entity directEntity, Entity causingEntity) {
        this(type, directEntity, causingEntity, null);
    }
    public DamageSource(Holder<DamageType> type, Vec3 damageSourcePosition) {
        this(type, null, null, damageSourcePosition);
    }
    public DamageSource(Holder<DamageType> type, Entity causingEntity) {
        this(type, causingEntity, causingEntity, null);
    }
    public DamageSource(Holder<DamageType> type) {
        this(type, null, null, null);
    }
    public Entity getDirectEntity() {
        return directEntity;
    }
    public Entity getEntity() {
        return causingEntity;
    }
    public ItemStack getWeaponItem() {
        return directEntity != null ? directEntity.getWeaponItem() : null;
    }
    public Component getLocalizedDeathMessage(LivingEntity victim) {
        String key = "death.attack." + getMsgId();
        if (causingEntity == null && directEntity == null) {
            LivingEntity credit = victim.getKillCredit();
            return credit != null
                ? Component.translatable(key + ".player", victim.getDisplayName(), credit.getDisplayName())
                : Component.translatable(key, victim.getDisplayName());
        }
        Component attacker = (causingEntity != null ? causingEntity : directEntity).getDisplayName();
        ItemStack held = causingEntity instanceof LivingEntity living ? living.getMainHandItem() : ItemStack.EMPTY;
        if (!held.isEmpty() && held.has(net.minecraft.core.component.DataComponents.CUSTOM_NAME)) {
            return Component.translatable(key + ".item", victim.getDisplayName(), attacker, held.getDisplayName());
        }
        return Component.translatable(key, victim.getDisplayName(), attacker);
    }
    public String getMsgId() {
        return type.value().msgId();
    }
    public boolean scalesWithDifficulty() {
        boolean livingNonPlayer = causingEntity instanceof LivingEntity && !(causingEntity instanceof net.minecraft.world.entity.player.Player);
        return org.basaltmc.bukkit.MechanicsNatives.scalesWithDifficulty(type.value().scaling().name(), livingNonPlayer);
    }
    public boolean isCreativePlayer() {
        return causingEntity instanceof net.minecraft.world.entity.player.Player player && player.getAbilities().instabuild;
    }
    public Vec3 getSourcePosition() {
        return damageSourcePosition != null ? damageSourcePosition : directEntity != null ? directEntity.position() : null;
    }
    public Vec3 sourcePositionRaw() {
        return damageSourcePosition;
    }
    public boolean is(TagKey<DamageType> tag) {
        return isInTag("#" + tag.location());
    }
    public boolean isInTag(String tag) {
        return type != null && type.unwrapKey().isPresent()
            && org.basaltmc.bukkit.item.TagSets.members("damage_type", tag).contains(type.unwrapKey().get().identifier().toString());
    }
    public boolean is(ResourceKey<DamageType> typeKey) {
        return type != null && type.unwrapKey().equals(java.util.Optional.of(typeKey));
    }
    public DamageType type() {
        return type.value();
    }
    public Holder<DamageType> typeHolder() {
        return type;
    }
}
