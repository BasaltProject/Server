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
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.damagesource.DamageSource#getFoodExhaustion");
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
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.damagesource.DamageSource#getWeaponItem");
    }
    public Component getLocalizedDeathMessage(LivingEntity victim) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.damagesource.DamageSource#getLocalizedDeathMessage");
    }
    public String getMsgId() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.damagesource.DamageSource#getMsgId");
    }
    public boolean scalesWithDifficulty() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.damagesource.DamageSource#scalesWithDifficulty");
    }
    public boolean isCreativePlayer() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.damagesource.DamageSource#isCreativePlayer");
    }
    public Vec3 getSourcePosition() {
        return damageSourcePosition != null ? damageSourcePosition : directEntity != null ? directEntity.position() : null;
    }
    public Vec3 sourcePositionRaw() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.damagesource.DamageSource#sourcePositionRaw");
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
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.damagesource.DamageSource#type");
    }
    public Holder<DamageType> typeHolder() {
        return type;
    }
}
