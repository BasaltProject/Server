package org.basaltmc.registry.tags

import org.basaltmc.common.Identifier

/** Thrown when a tag graph can't be resolved: a required reference is missing, or a cycle exists. */
public class TagResolutionException(
    message: String,
) : Exception(message)

/**
 * Flattens a set of [TagFile]s (all belonging to one registry's tag directory) into resolved
 * element-identifier sets. Mirrors vanilla's `TagLoader.build`: tags can reference other tags, so a
 * tag is resolved by recursively resolving (and flattening) the tags it references first, and a
 * missing required element or tag reference is an error while a missing optional one is dropped
 * silently. Vanilla does this via a topological sort of the whole graph up front; a memoized
 * depth-first resolution reaches the same result (each tag still resolved exactly once, referenced
 * tags always before their dependents) without needing the graph pre-sorted.
 */
public object TagSet {
    public fun resolve(
        files: Map<Identifier, TagFile>,
        elementExists: (Identifier) -> Boolean,
    ): Map<Identifier, Set<Identifier>> {
        val resolved = LinkedHashMap<Identifier, Set<Identifier>>()
        val inProgress = mutableSetOf<Identifier>()

        fun resolveTag(id: Identifier): Set<Identifier> {
            resolved[id]?.let { return it }
            if (id in inProgress) throw TagResolutionException("Cycle detected while resolving tag $id")
            val file = files[id] ?: throw TagResolutionException("Unknown tag $id")
            inProgress += id

            val values = LinkedHashSet<Identifier>()
            for (entry in file.entries) {
                if (entry.isTag) {
                    if (entry.id !in files) {
                        if (entry.required) throw TagResolutionException("Tag $id references missing tag ${entry.id}")
                        continue
                    }
                    values += resolveTag(entry.id)
                } else {
                    if (!elementExists(entry.id)) {
                        if (entry.required) {
                            throw TagResolutionException(
                                "Tag $id references missing element ${entry.id}",
                            )
                        }
                        continue
                    }
                    values += entry.id
                }
            }

            inProgress -= id
            resolved[id] = values
            return values
        }

        for (id in files.keys) resolveTag(id)
        return resolved
    }
}
