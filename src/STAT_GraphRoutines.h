#include "graphlib.h"
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#ifndef STAT_GRAPHROUTINES_H
#define STAT_GRAPHROUTINES_H

#define STAT_GRAPH_CHUNK 8192

//! The scalar data type that makes up the bit vector
typedef int64_t StatBitVector_t;
#define STAT_BITVECTOR_BITS 64 
#define STAT_BITVECTOR_BYTES 8
#define STAT_GRAPH_BIT(i) ((StatBitVector_t)1<<(i))

//! A struct to specify STAT's bit vector edge label type
typedef struct
{
    size_t length;
    StatBitVector_t *bitVector;
} StatBitVectorEdge_t;

//! Initialize the standard bit vector functions
void statInitializeBitVectorFunctions();

//! Initialize the merge bit vector functions for the filter
void statInitializeMergeFunctions();

//! Initialize the bit vector reorder functions for the frontend
void statInitializeReorderFunctions();

//! Calculate the bit vector length
/*!
    \param numTasks - the number of tasks to represent in the bit vector
    \return the length of the bit vector (i.e., number of StatBitVector_t elements)
*/
size_t statBitVectorLength(int numTasks);

//! Serialize the STAT node object into a buffer
/*!
    \param buf - the buffer to write to
    \param node - a pointer to the node object
*/
void statSerializeNode(char *buf, const void *node);

//! Calculate the serialized length of the STAT node object
/*!
    \param node - a pointer to the node object
    \return the serialized length of the node
*/
unsigned int statSerializeNodeLength(const void *node);

//! Deserialize the STAT node object from a buffer
/*!
    \param[out] node - a pointer to the node object
    \param buf - the serialized buffer
    \param bufLength - the length of the serialized buffer
*/
void statDeserializeNode(void **node, const char *buf, unsigned int bufLength);

//! Translate the STAT node object into a string
/*!
    \param node - a pointer to the node object
    \return the string representation of the node
*/
char *statNodeToText(const void *node);

//! Merge two STAT node objects
/*!
    \param[inout] node1 - the node to merge into
    \param node2 - the node to merge in
*/
void statMergeNode(void *node1, const void *node2);

//! Copy a STAT node object
/*!
    \param node - a pointer to the node object to be copied
    \return a pointer to the new node copy
*/
void *statCopyNode(const void *node);

//! Free a STAT node object
/*!
    \param node - a pointer to the node object to be freed
*/
void statFreeNode(void *node);

//! Serialize a STAT edge object into a buffer
/*!
    \param buf - the buffer to write to
    \param edge - a pointer to the edge object
*/
void statSerializeEdge(char *buf, const void *edge);

//! Calculate the serialized length of a STAT edge object
/*!
    \param edge - a pointer to the edge object
    \return the serialized length of the edge
*/
unsigned int statSerializeEdgeLength(const void *edge);

//! Deserialize a STAT edge object from a buffer
/*!
    \param[out] edge - a pointer to the edge object
    \param buf - the serialized buffer
    \param bufLength - the length of the serialized buffer
*/
void statDeserializeEdge(void **edge, const char *buf, unsigned int bufLength);

//! Translate a STAT edge object into a string
/*!
    \param edge - a pointer to the edge object
    \return the string representation of the edge
*/
char *statEdgeToText(const void *edge);

//! Merge two STAT edge objects
/*!
    \param[inout] edge1 - the edge to merge into
    \param edge2 - the edge to merge in
*/
void statMergeEdge(void *edge1, const void *edge2);

//! Copy a STAT edge object
/*!
    \param edge - a pointer to the edge object to be copied
    \return a pointer to the new edge copy
*/
void *statCopyEdge(const void *edge);

//! Free a STAT edge object
/*!
    \param edge - a pointer to the edge object to be freed
*/
void statFreeEdge(void *edge);

//! Calculate a checksum of a STAT edge object
/*!
    \param edge - a pointer to the edge object
    \return the checksum of the edge
*/
long statEdgeCheckSum(const void *edge);

//! Deserialize a STAT edge object from a buffer into the appropriate location in the bit vector for the STAT filter.This requires statGraphRoutinesTotalWidth must be set to the final bit vector width, statGraphRoutinesEdgeLAbelWidths must be a list of bit vector widths, and statGraphRoutinesCurrentIndex must be set to the current index.
/*!
    \param[out] edge - a pointer to the edge object
    \param buf - the serialized buffer
    \param bufLength - the length of the serialized buffer
*/
void statFilterDeserializeEdge(void **edge, const char *buf, unsigned int bufLength);


//! Initialize an empty bit vector for initializing the frontend's reorder graph
/*!
    \param edge - a pointer to the edge object to be copied
    \return a pointer to the new edge copy
*/
void *statCopyEdgeInitializeEmpty(const void *edge);


//! Merge the unordered input vector bits into the appropriate in-order bits of the frontend's reorder graph. This requires statGraphRoutinesRanksList must be set to the ranks list for the current bitvector, statGraphRoutinesRanksListLength must be set to the length of the ranks list for the current bitvector, and statGraphRoutinesCurrentIndex must be set to the start index for the bit vector in edge
/*!
    \param[inout] edge1 - the edge to merge into
    \param edge2 - the edge to merge in
*/
void statMergeEdgeOrdered(void *edge1, const void *edge2);

#endif
