package io.papermc.paper.datacomponent.item;

import java.util.LinkedHashMap;
import java.util.Map;
import org.basaltmc.bukkit.datacomponent.Builders;
import org.basaltmc.bukkit.datacomponent.ContentSpecs;
import org.basaltmc.bukkit.datacomponent.ItemSpecs;

public final class BasaltComponentBridge implements ItemComponentTypesBridge {
    @Override
    public ItemLore.Builder lore() {
        return Builders.lore();
    }

    @Override
    public ItemEnchantments.Builder enchantments() {
        return Builders.enchantments();
    }

    @Override
    public FoodProperties.Builder food() {
        return Builders.food();
    }

    @Override
    public DyedItemColor.Builder dyedItemColor() {
        return Builders.dyed();
    }

    @Override
    public CustomModelData.Builder customModelData() {
        return Builders.model();
    }

    @Override
    public MapId mapId(int id) {
        return Builders.mapId(id);
    }

    @Override
    public UseRemainder useRemainder(org.bukkit.inventory.ItemStack itemStack) {
        return Builders.remainder(itemStack);
    }

    @Override
    public UseCooldown.Builder useCooldown(float seconds) {
        return Builders.cooldown(seconds);
    }

    @Override
    public DamageResistant damageResistant(io.papermc.paper.registry.set.RegistryKeySet<org.bukkit.damage.DamageType> types) {
        return Builders.damageResistant(types);
    }

    @Override
    public Enchantable enchantable(int level) {
        return Builders.enchantable(level);
    }

    @Override
    public Repairable repairable(io.papermc.paper.registry.set.RegistryKeySet<org.bukkit.inventory.ItemType> types) {
        return Builders.repairable(types);
    }

    @Override
    public OminousBottleAmplifier ominousBottleAmplifier(int amplifier) {
        return Builders.amplifier(amplifier);
    }

    @Override
    public TooltipDisplay.Builder tooltipDisplay() {
        return Builders.tooltip();
    }

    @Override
    public ChargedProjectiles.Builder chargedProjectiles() {
        return (ChargedProjectiles.Builder) ContentSpecs.CHARGED.builder();
    }

    @Override
    public PotDecorations.Builder potDecorations() {
        return (PotDecorations.Builder) ContentSpecs.DECORATIONS.builder();
    }

    @Override
    public ItemAttributeModifiers.Builder modifiers() {
        return (ItemAttributeModifiers.Builder) ItemSpecs.MODIFIERS.builder();
    }

    @Override
    public PotionContents.Builder potionContents() {
        return (PotionContents.Builder) ContentSpecs.POTION.builder();
    }

    @Override
    public BundleContents.Builder bundleContents() {
        return (BundleContents.Builder) ContentSpecs.BUNDLE.builder();
    }

    @Override
    public SuspiciousStewEffects.Builder suspiciousStewEffects() {
        return (SuspiciousStewEffects.Builder) ContentSpecs.STEW.builder();
    }

    @Override
    public MapDecorations.Builder mapDecorations() {
        return (MapDecorations.Builder) ContentSpecs.MAP_DECORATIONS.builder();
    }

    @Override
    public MapDecorations.DecorationEntry mapDecorationEntry(org.bukkit.map.MapCursor.Type type, double x, double z, float rotation) {
        return (MapDecorations.DecorationEntry) ContentSpecs.decoration(type, x, z, rotation);
    }

    @Override
    public SeededContainerLoot.Builder seededContainerLoot(net.kyori.adventure.key.Key lootTableKey) {
        Map<String, Object> fields = new LinkedHashMap<>();
        fields.put("lootTable", lootTableKey);
        return (SeededContainerLoot.Builder) org.basaltmc.bukkit.datacomponent.Dyn.builder(ContentSpecs.LOOT, fields);
    }

    @Override
    public WrittenBookContent.Builder writtenBookContent(io.papermc.paper.text.Filtered<String> title, String author) {
        Map<String, Object> fields = new LinkedHashMap<>();
        fields.put("title", title);
        fields.put("author", author);
        return (WrittenBookContent.Builder) org.basaltmc.bukkit.datacomponent.Dyn.builder(ContentSpecs.WRITTEN, fields);
    }

    @Override
    public WritableBookContent.Builder writeableBookContent() {
        return (WritableBookContent.Builder) ContentSpecs.WRITABLE.builder();
    }

    @Override
    public ItemArmorTrim.Builder itemArmorTrim(org.bukkit.inventory.meta.trim.ArmorTrim armorTrim) {
        Map<String, Object> fields = new LinkedHashMap<>();
        fields.put("armorTrim", armorTrim);
        return (ItemArmorTrim.Builder) org.basaltmc.bukkit.datacomponent.Dyn.builder(ContentSpecs.TRIM, fields);
    }

    @Override
    public LodestoneTracker.Builder lodestoneTracker() {
        return (LodestoneTracker.Builder) ContentSpecs.LODESTONE.builder();
    }

    @Override
    public Fireworks.Builder fireworks() {
        return (Fireworks.Builder) ContentSpecs.FIREWORKS.builder();
    }

    @Override
    public ResolvableProfile resolvableProfile(com.destroystokyo.paper.profile.PlayerProfile profile) {
        Map<String, Object> fields = new LinkedHashMap<>();
        fields.put("name", profile.getName());
        fields.put("uuid", profile.getId());
        fields.put("properties", new java.util.ArrayList<Object>(profile.getProperties()));
        return (ResolvableProfile) ContentSpecs.PROFILE.value(fields);
    }

    @Override
    public ResolvableProfile.Builder resolvableProfile() {
        return (ResolvableProfile.Builder) ContentSpecs.PROFILE.builder();
    }

    @Override
    public ResolvableProfile.SkinPatchBuilder skinPatch() {
        return (ResolvableProfile.SkinPatchBuilder) ContentSpecs.PATCH.builder();
    }

    @Override
    public ResolvableProfile.SkinPatch emptySkinPatch() {
        return (ResolvableProfile.SkinPatch) ContentSpecs.PATCH.value(new LinkedHashMap<>());
    }

    @Override
    public BannerPatternLayers.Builder bannerPatternLayers() {
        return (BannerPatternLayers.Builder) ContentSpecs.LAYERS.builder();
    }

    @Override
    public BlockItemDataProperties.Builder blockItemStateProperties() {
        return (BlockItemDataProperties.Builder) ContentSpecs.BLOCK_DATA.builder();
    }

    @Override
    public ItemContainerContents.Builder itemContainerContents() {
        return (ItemContainerContents.Builder) ContentSpecs.CONTAINER.builder();
    }

    @Override
    public JukeboxPlayable.Builder jukeboxPlayable(org.bukkit.JukeboxSong song) {
        Map<String, Object> fields = new LinkedHashMap<>();
        fields.put("jukeboxSong", song);
        return (JukeboxPlayable.Builder) org.basaltmc.bukkit.datacomponent.Dyn.builder(ContentSpecs.JUKEBOX, fields);
    }

    @Override
    public Tool.Builder tool() {
        return (Tool.Builder) ItemSpecs.TOOL.builder();
    }

    @Override
    public Tool.Rule toolRule(io.papermc.paper.registry.set.RegistryKeySet<org.bukkit.block.BlockType> blocks, Float speed, net.kyori.adventure.util.TriState correctForDrops) {
        Map<String, Object> fields = new LinkedHashMap<>();
        fields.put("blocks", blocks);
        if (speed != null) {
            fields.put("speed", speed);
        }
        fields.put("correctForDrops", correctForDrops == null ? net.kyori.adventure.util.TriState.NOT_SET : correctForDrops);
        return (Tool.Rule) ItemSpecs.RULE.value(fields);
    }

    @Override
    public ItemAdventurePredicate.Builder itemAdventurePredicate() {
        return (ItemAdventurePredicate.Builder) ContentSpecs.PREDICATE.builder();
    }

    @Override
    public Consumable.Builder consumable() {
        return (Consumable.Builder) ItemSpecs.CONSUMABLE.builder();
    }

    @Override
    public Equippable.Builder equippable(org.bukkit.inventory.EquipmentSlot slot) {
        return (Equippable.Builder) ItemSpecs.equippable(slot);
    }

    @Override
    public DeathProtection.Builder deathProtection() {
        return (DeathProtection.Builder) ItemSpecs.DEATH_PROTECTION.builder();
    }

    @Override
    public BlocksAttacks.Builder blocksAttacks() {
        return (BlocksAttacks.Builder) ItemSpecs.BLOCKS_ATTACKS.builder();
    }

    @Override
    public Weapon.Builder weapon() {
        return (Weapon.Builder) ItemSpecs.WEAPON.builder();
    }

    @Override
    public KineticWeapon.Builder kineticWeapon() {
        return (KineticWeapon.Builder) ItemSpecs.KINETIC.builder();
    }

    @Override
    public KineticWeapon.Condition kineticWeaponCondition(int maxDurationTicks, float minSpeed, float minRelativeSpeed) {
        return (KineticWeapon.Condition) ItemSpecs.condition(maxDurationTicks, minSpeed, minRelativeSpeed);
    }

    @Override
    public UseEffects.Builder useEffects() {
        return (UseEffects.Builder) ItemSpecs.USE_EFFECTS.builder();
    }

    @Override
    public PiercingWeapon.Builder piercingWeapon() {
        return (PiercingWeapon.Builder) ItemSpecs.PIERCING.builder();
    }

    @Override
    public AttackRange.Builder attackRange() {
        return (AttackRange.Builder) ItemSpecs.ATTACK_RANGE.builder();
    }

    @Override
    public SwingAnimation.Builder swingAnimation() {
        return (SwingAnimation.Builder) ItemSpecs.SWING.builder();
    }

    @Override
    public SulfurCubeContent sulfurCubeContent(org.bukkit.inventory.ItemStack stack) {
        Map<String, Object> fields = new LinkedHashMap<>();
        fields.put("absorbedItem", stack.clone());
        return (SulfurCubeContent) ContentSpecs.SULFUR.value(fields);
    }

    @Override
    public MobVisibility mobVisibility(io.papermc.paper.registry.set.RegistryKeySet<org.bukkit.entity.EntityType> types, float multiplier) {
        Map<String, Object> fields = new LinkedHashMap<>();
        fields.put("targetingEntityTypes", types);
        fields.put("visibility", multiplier);
        return (MobVisibility) ContentSpecs.VISIBILITY.value(fields);
    }

    @Override
    public VillagerFood villagerFood(int value) {
        Map<String, Object> fields = new LinkedHashMap<>();
        fields.put("nutrition", value);
        return (VillagerFood) ContentSpecs.FOOD.value(fields);
    }

    @Override
    public SignText.Builder signText() {
        return (SignText.Builder) ContentSpecs.SIGN.builder();
    }
}
