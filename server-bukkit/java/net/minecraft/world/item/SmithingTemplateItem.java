// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;
import java.util.List;
import java.util.function.Consumer;
import net.minecraft.ChatFormatting;
import net.minecraft.network.chat.CommonComponents;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.Identifier;
import net.minecraft.world.item.component.TooltipDisplay;
public class SmithingTemplateItem extends Item {
    private static Component text(String key, ChatFormatting format) {
        Component component = Component.translatable("item.minecraft." + key);
        return format == null ? component : ((net.minecraft.network.chat.MutableComponent) component).withStyle(format);
    }
    private static Identifier slot(String name) {
        return Identifier.withDefaultNamespace("container/slot/" + name);
    }
    private static final Component kIngredientsTitle = text("smithing_template.ingredients", ChatFormatting.GRAY);
    private static final Component kAppliesToTitle = text("smithing_template.applies_to", ChatFormatting.GRAY);
    private static final Component kSuffix = text("smithing_template", ChatFormatting.GRAY);
    private final Component appliesTo;
    private final Component ingredients;
    private final Component baseSlotDescription;
    private final Component additionsSlotDescription;
    private final List<Identifier> baseSlotEmptyIcons;
    private final List<Identifier> additionalSlotEmptyIcons;
    public SmithingTemplateItem(Component appliesTo, Component ingredients, Component baseSlotDescription, Component additionsSlotDescription, List<Identifier> baseSlotEmptyIcons, List<Identifier> additionalSlotEmptyIcons, Item.Properties properties) {
        super(properties);
        this.appliesTo = appliesTo;
        this.ingredients = ingredients;
        this.baseSlotDescription = baseSlotDescription;
        this.additionsSlotDescription = additionsSlotDescription;
        this.baseSlotEmptyIcons = baseSlotEmptyIcons;
        this.additionalSlotEmptyIcons = additionalSlotEmptyIcons;
    }
    public static SmithingTemplateItem createArmorTrimTemplate(Item.Properties properties) {
        return new SmithingTemplateItem(text("smithing_template.armor_trim.applies_to", ChatFormatting.BLUE), text("smithing_template.armor_trim.ingredients", ChatFormatting.BLUE),
            text("smithing_template.armor_trim.base_slot_description", null), text("smithing_template.armor_trim.additions_slot_description", null),
            List.of(slot("helmet"), slot("chestplate"), slot("leggings"), slot("boots")),
            List.of(slot("ingot"), slot("redstone_dust"), slot("lapis_lazuli"), slot("quartz"), slot("diamond"), slot("emerald"), slot("amethyst_shard")), properties);
    }
    public static SmithingTemplateItem createNetheriteUpgradeTemplate(Item.Properties properties) {
        return new SmithingTemplateItem(text("smithing_template.netherite_upgrade.applies_to", ChatFormatting.BLUE), text("smithing_template.netherite_upgrade.ingredients", ChatFormatting.BLUE),
            text("smithing_template.netherite_upgrade.base_slot_description", null), text("smithing_template.netherite_upgrade.additions_slot_description", null),
            List.of(slot("helmet"), slot("sword"), slot("chestplate"), slot("pickaxe"), slot("leggings"), slot("axe"), slot("boots"), slot("hoe"), slot("shovel"), slot("nautilus_armor"), slot("spear")),
            List.of(slot("ingot")), properties);
    }
    public void appendHoverText(ItemStack itemStack, Item.TooltipContext context, TooltipDisplay display, Consumer<Component> builder, TooltipFlag tooltipFlag) {
        builder.accept(kSuffix);
        builder.accept(CommonComponents.EMPTY);
        builder.accept(kAppliesToTitle);
        builder.accept(CommonComponents.space().append(appliesTo));
        builder.accept(kIngredientsTitle);
        builder.accept(CommonComponents.space().append(ingredients));
    }
    public Component getBaseSlotDescription() {
        return baseSlotDescription;
    }
    public Component getAdditionSlotDescription() {
        return additionsSlotDescription;
    }
    public List<Identifier> getBaseSlotEmptyIcons() {
        return baseSlotEmptyIcons;
    }
    public List<Identifier> getAdditionalSlotEmptyIcons() {
        return additionalSlotEmptyIcons;
    }
}
