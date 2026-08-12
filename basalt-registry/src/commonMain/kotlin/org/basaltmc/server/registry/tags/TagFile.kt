package org.basaltmc.server.registry.tags

import org.basaltmc.wire.common.Identifier

/**
 * One entry in a tag file's `values` list: either a concrete element or a reference to another
 * tag (leading `#`), optionally [required]. Mirrors vanilla's `TagEntry`/`ExtraCodecs.TAG_OR_ELEMENT_ID`.
 */
public data class TagEntry(
    val id: Identifier,
    val isTag: Boolean,
    val required: Boolean = true,
)

/** One `tags/<registry>/<path>.json` file. Mirrors vanilla's `TagFile` record. */
public data class TagFile(
    val entries: List<TagEntry>,
    val replace: Boolean = false,
)
