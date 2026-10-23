package io.papermc.paper.datacomponent.item;

import org.basaltmc.bukkit.Unsupported;
import org.basaltmc.bukkit.datacomponent.Builders;

/** The factories the item component interfaces reach the server through; the kinds not held yet say so on use. */
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
        throw Unsupported.hit("ChargedProjectiles");
    }

    @Override
    public PotDecorations.Builder potDecorations() {
        throw Unsupported.hit("PotDecorations");
    }

    @Override
    public ItemAttributeModifiers.Builder modifiers() {
        throw Unsupported.hit("ItemAttributeModifiers");
    }

    @Override
    public PotionContents.Builder potionContents() {
        throw Unsupported.hit("PotionContents");
    }

    @Override
    public BundleContents.Builder bundleContents() {
        throw Unsupported.hit("BundleContents");
    }

    @Override
    public SuspiciousStewEffects.Builder suspiciousStewEffects() {
        throw Unsupported.hit("SuspiciousStewEffects");
    }

    @Override
    public MapDecorations.Builder mapDecorations() {
        throw Unsupported.hit("MapDecorations");
    }

    @Override
    public MapDecorations.DecorationEntry mapDecorationEntry(org.bukkit.map.MapCursor.Type type, double x, double z, float rotation) {
        throw Unsupported.hit("MapDecorations");
    }

    @Override
    public SeededContainerLoot.Builder seededContainerLoot(net.kyori.adventure.key.Key lootTableKey) {
        throw Unsupported.hit("SeededContainerLoot");
    }

    @Override
    public WrittenBookContent.Builder writtenBookContent(io.papermc.paper.text.Filtered<String> title, String author) {
        throw Unsupported.hit("WrittenBookContent");
    }

    @Override
    public WritableBookContent.Builder writeableBookContent() {
        throw Unsupported.hit("WritableBookContent");
    }

    @Override
    public ItemArmorTrim.Builder itemArmorTrim(org.bukkit.inventory.meta.trim.ArmorTrim armorTrim) {
        throw Unsupported.hit("ItemArmorTrim");
    }

    @Override
    public LodestoneTracker.Builder lodestoneTracker() {
        throw Unsupported.hit("LodestoneTracker");
    }

    @Override
    public Fireworks.Builder fireworks() {
        throw Unsupported.hit("Fireworks");
    }

    @Override
    public ResolvableProfile resolvableProfile(com.destroystokyo.paper.profile.PlayerProfile profile) {
        throw Unsupported.hit("ResolvableProfile");
    }

    @Override
    public ResolvableProfile.Builder resolvableProfile() {
        throw Unsupported.hit("ResolvableProfile");
    }

    @Override
    public ResolvableProfile.SkinPatchBuilder skinPatch() {
        throw Unsupported.hit("ResolvableProfile");
    }

    @Override
    public ResolvableProfile.SkinPatch emptySkinPatch() {
        throw Unsupported.hit("ResolvableProfile");
    }

    @Override
    public BannerPatternLayers.Builder bannerPatternLayers() {
        throw Unsupported.hit("BannerPatternLayers");
    }

    @Override
    public BlockItemDataProperties.Builder blockItemStateProperties() {
        throw Unsupported.hit("BlockItemDataProperties");
    }

    @Override
    public ItemContainerContents.Builder itemContainerContents() {
        throw Unsupported.hit("ItemContainerContents");
    }

    @Override
    public JukeboxPlayable.Builder jukeboxPlayable(org.bukkit.JukeboxSong song) {
        throw Unsupported.hit("JukeboxPlayable");
    }

    @Override
    public Tool.Builder tool() {
        throw Unsupported.hit("Tool");
    }

    @Override
    public Tool.Rule toolRule(io.papermc.paper.registry.set.RegistryKeySet<org.bukkit.block.BlockType> blocks, Float speed, net.kyori.adventure.util.TriState correctForDrops) {
        throw Unsupported.hit("Tool");
    }

    @Override
    public ItemAdventurePredicate.Builder itemAdventurePredicate() {
        throw Unsupported.hit("ItemAdventurePredicate");
    }

    @Override
    public Consumable.Builder consumable() {
        throw Unsupported.hit("Consumable");
    }

    @Override
    public Equippable.Builder equippable(org.bukkit.inventory.EquipmentSlot slot) {
        throw Unsupported.hit("Equippable");
    }

    @Override
    public DeathProtection.Builder deathProtection() {
        throw Unsupported.hit("DeathProtection");
    }

    @Override
    public BlocksAttacks.Builder blocksAttacks() {
        throw Unsupported.hit("BlocksAttacks");
    }

    @Override
    public Weapon.Builder weapon() {
        throw Unsupported.hit("Weapon");
    }

    @Override
    public KineticWeapon.Builder kineticWeapon() {
        throw Unsupported.hit("KineticWeapon");
    }

    @Override
    public KineticWeapon.Condition kineticWeaponCondition(int maxDurationTicks, float minSpeed, float minRelativeSpeed) {
        throw Unsupported.hit("KineticWeapon");
    }

    @Override
    public UseEffects.Builder useEffects() {
        throw Unsupported.hit("UseEffects");
    }

    @Override
    public PiercingWeapon.Builder piercingWeapon() {
        throw Unsupported.hit("PiercingWeapon");
    }

    @Override
    public AttackRange.Builder attackRange() {
        throw Unsupported.hit("AttackRange");
    }

    @Override
    public SwingAnimation.Builder swingAnimation() {
        throw Unsupported.hit("SwingAnimation");
    }

    @Override
    public SulfurCubeContent sulfurCubeContent(org.bukkit.inventory.ItemStack stack) {
        throw Unsupported.hit("SulfurCubeContent");
    }

    @Override
    public MobVisibility mobVisibility(io.papermc.paper.registry.set.RegistryKeySet<org.bukkit.entity.EntityType> types, float multiplier) {
        throw Unsupported.hit("MobVisibility");
    }

    @Override
    public VillagerFood villagerFood(int value) {
        throw Unsupported.hit("VillagerFood");
    }

    @Override
    public SignText.Builder signText() {
        throw Unsupported.hit("SignText");
    }
}
