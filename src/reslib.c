/* -*- C -*- */
/* I hate this bloody country. Smash. */
/* This file is part of ResLib project */

#ifdef HAVE_CONFIG_H
# include <rlconfig.h>
#endif /* HAVE_CONFIG_H */

#define __USE_GNU
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdarg.h>

#include <tsearch.h>

#define RL_MODE PROTO /* explicitly set type of inclusion */
#include <reslib.h>

#undef RL_MODE
#define RL_MODE DESC /* we'll need descriptors of our own types */
#include <rlprotos.h>

static void * rl_malloc (const char * filename, const char * function, int line, size_t size) { return (malloc (size)); }
static void * rl_realloc (const char * filename, const char * function, int line, void * ptr, size_t size) { return (realloc (ptr, size)); }
static char * rl_strdup (const char * filename, const char * function, int line, const char * str) { return (strdup (str)); }
static void rl_free (const char * filename, const char * function, int line, void * ptr) { free (ptr); }

/** ResLib configuration structure */
rl_conf_t rl_conf = {
  .rl_mem = { /**< all memory functions may be replaced on user defined */
    .mem_alloc_strategy = 2, /**< Memory allocation strategy. Default is to double buffer every time. */
    .malloc = rl_malloc, /**< Pointer to malloc function. */
    .realloc = rl_realloc, /**< Pointer to realloc function. */
    .strdup = rl_strdup, /**< Pointer to strdup function. */
    .free = rl_free, /**< Pointer to free function. */
  },
  .log_level = RL_LL_ALL, /**< default log level ALL */
  .msg_handler = NULL, /**< pointer on user defined message handler */
  .des = {
    .data = NULL,
    .size = 0,
    .alloc_size = 0,
  },
#ifndef RL_TREE_LOOKUP
  .hash = {
    .ra = { /**< resizable array with hash for type descriptors */
      .data = NULL, /* descriptors rl_rarray_t */
      .size = 0,
      .alloc_size = 0,
    },
  },
#else /* RL_TREE_LOOKUP */
  .tree = NULL,
#endif /* RL_TREE_LOOKUP */
  .allocated_mem = { /**< resizable array with strings for cleanup */
    .data = NULL, /* descriptors rl_rarray_t */
    .size = 0,
    .alloc_size = 0,
  },
  .enum_by_name = NULL,
};

rl_io_handler_t rl_io_handlers[RL_MAX_TYPES] =
  { [0 ... RL_MAX_TYPES - 1] =
    {
      .load =
      {
	.rl = NULL,
	.xdr = NULL,
      },
      .save =
      {
	.rl = NULL,
	.xdr = NULL,
	.xml = NULL,
	.cinit = NULL,
	.scm = NULL,
      },
    }
  };
rl_io_handler_t rl_io_ext_handlers[RL_MAX_TYPES] =
  { [0 ... RL_MAX_TYPES - 1] =
    {
      .load =
      {
	.rl = NULL,
	.xdr = NULL,
      },
      .save =
      {
	.rl = NULL,
	.xdr = NULL,
	.xml = NULL,
	.cinit = NULL,
	.scm = NULL,
      },
    }
  };

RL_MEM_INIT ( , __attribute__((constructor,weak)));

/**
 * Memory cleanp handler.
 */
static void __attribute__((destructor)) rl_cleanup (void)
{
  void dummy_free_func (void * nodep) {}
  
  int free_lookup_tree (rl_td_t * tdp, void * arg)
  {
    tdestroy (tdp->lookup_by_value, dummy_free_func);
    tdp->lookup_by_value = NULL;
    if (tdp->lookup_by_name.data)
      RL_FREE (tdp->lookup_by_name.data);
    tdp->lookup_by_name.data = NULL;
    tdp->lookup_by_name.size = tdp->lookup_by_name.alloc_size = 0;
    return (0);
  }
  
  tdestroy (rl_conf.enum_by_name, dummy_free_func);
  rl_conf.enum_by_name = NULL;
  rl_td_foreach (free_lookup_tree, NULL);
  
#ifndef RL_TREE_LOOKUP
  if (rl_conf.hash.ra.data)
    RL_FREE (rl_conf.hash.ra.data);
  rl_conf.hash.ra.data = NULL;
  rl_conf.hash.ra.size = rl_conf.hash.ra.alloc_size = 0;
#else /* RL_TREE_LOOKUP */
  tdestroy (rl_conf.tree, dummy_free_func);
  rl_conf.tree = NULL;
#endif /* RL_TREE_LOOKUP */

  if (rl_conf.des.data)
    RL_FREE (rl_conf.des.data);
  rl_conf.des.data = NULL;
  rl_conf.des.size = rl_conf.des.alloc_size = 0;

  if (rl_conf.allocated_mem.data)
    {
      int i;
      for (i = 0; i < rl_conf.allocated_mem.size / sizeof (rl_conf.allocated_mem.data[0]); ++i)
	if (rl_conf.allocated_mem.data[i].ptr)
	  RL_FREE (rl_conf.allocated_mem.data[i].ptr);
      RL_FREE (rl_conf.allocated_mem.data);
      rl_conf.allocated_mem.data = NULL;
    }
  rl_conf.allocated_mem.size = rl_conf.allocated_mem.alloc_size = 0;
}

void
rl_message_format (void (*output_handler) (char*), rl_message_id_t message_id, va_list args)
{
  static const char * messages[RL_MESSAGE_LAST + 1] = { [0 ... RL_MESSAGE_LAST] = NULL };
  static int messages_inited = 0;
  const char * format = "Unknown RL_MESSAGE_ID.";
  char * str = NULL;

  if (!messages_inited)
    {
      rl_td_t * tdp = rl_get_td_by_name ("rl_message_id_t");
      if (tdp)
	{
	  int i;
	  rl_fd_t * fdp = tdp->fields.data;
	  for (i = 0; fdp[i].rl_type != RL_TYPE_TRAILING_RECORD; ++i)
	    messages[fdp[i].value] = fdp[i].comment;
	}
      messages_inited = !0;
    }

  if ((message_id >= 0) && (message_id <= sizeof (messages) / sizeof (messages[0])) && messages[message_id])
    format = messages[message_id];

  vasprintf (&str, format, args);

  if (str)
    {
      output_handler (str);
      free (str);
    }
}

/**
 * Redirect message to user defined handler or output message to stderr.
 * @param file_name file name 
 * @param func_name function name 
 * @param line line number
 * @param log_level logging level of message
 * @param message_id message template string ID
 */
void
rl_message (const char * file_name, const char * func_name, int line, rl_log_level_t log_level, rl_message_id_t message_id, ...)
{
  va_list args;
   
  va_start (args, message_id);
  /* if we have user defined message handler then pass error to it */
  if (rl_conf.msg_handler)
    rl_conf.msg_handler (file_name, func_name, line, log_level, message_id, args);
  else if (log_level > rl_conf.log_level)
    {
      const char * log_level_str_ = "Unknown";
#define LL_INIT(LEVEL) [RL_LL_##LEVEL] = #LEVEL
      static const char * log_level_str[] =
	{ LL_INIT (ALL), LL_INIT (TRACE), LL_INIT (DEBUG), LL_INIT (INFO), LL_INIT (WARN), LL_INIT (ERROR), LL_INIT (FATAL), LL_INIT (OFF) };


      void message_output (char * message)
      {
	fprintf (stderr, "%s: in %s %s() line %d: %s\n", log_level_str_, file_name, func_name, line, message);
	fflush (stderr);
      }
      
      if ((log_level >= 0) && (log_level <= sizeof (log_level_str) / sizeof (log_level_str[0])) && log_level_str[log_level])
	log_level_str_ = log_level_str[log_level];
      rl_message_format (message_output, message_id, args);
    }
  va_end (args);
}

#ifndef HAVE_STRNDUP
/**
 * Allocate new string and copy first 'size' chars from str.
 * For compilers without GNU extension
 *
 * @param str string to duplicate
 * @param size size to duplicate
 * @return A pointer on newly allocated string
 */
char *
strndup (const char * str, size_t size)
{
  char * copy;
  if (strlen (str) < size)
    size = strlen (str);
  copy = (char*)RL_MALLOC (size + 1);
  if (NULL == copy)
    {
      RL_MESSAGE (RL_LL_FATAL, RL_MESSAGE_OUT_OF_MEMORY);
      return (NULL);
    }    
  memcpy (copy, str, size);
  copy[size] = 0;
  return (copy);
}
#endif /* HAVE_STRNDUP */

/**
 * Rarray memory allocation/reallocation
 * @param rarray a pointer on resizable array
 * @param size size of array elements
 * @return Pointer on new element of rarray;
 */
void *
rl_rarray_append (rl_rarray_t * rarray, int size)
{
  if (NULL == rarray->data)
    {
      rarray->alloc_size = rarray->size = 0;
      rarray->data = RL_MALLOC (size);
      if (NULL == rarray->data)
	RL_MESSAGE (RL_LL_FATAL, RL_MESSAGE_OUT_OF_MEMORY);
      else
	{
	  memset (rarray->data, 0, size);
	  rarray->alloc_size = rarray->size = size;
	}
      return (rarray->data);
    }    

  rarray->size += size;
  if (rarray->size > rarray->alloc_size)
    {
      float mas = rl_conf.rl_mem.mem_alloc_strategy;
      int alloc_size;
      void * data;
      if (mas < 1)
	mas = 1;
      if (mas > 2)
	mas = 2;
      alloc_size = (((int)((rarray->alloc_size + 1) * mas) + size - 1) / size) * size;
      if (rarray->size > alloc_size)
	alloc_size = rarray->size;
      data = RL_REALLOC (rarray->data, alloc_size);
      if (NULL == data)
	{
	  RL_MESSAGE (RL_LL_FATAL, RL_MESSAGE_OUT_OF_MEMORY);
	  return (NULL);
	}
      rarray->alloc_size = alloc_size;
      rarray->data = data;
    }
  return (&((char*)rarray->data)[rarray->size - size]);
}

int __attribute__ ((format (printf, 2, 3))) 
rl_ra_printf (rl_rarray_t * rl_ra_str, const char * format, ...)
{
  va_list args;
  int length;
  char * str;
  char * tail;

  va_start (args, format);
  length = vasprintf (&str, format, args);
  va_end (args);
  if (NULL == str)
    {
      RL_MESSAGE (RL_LL_FATAL, RL_MESSAGE_OUT_OF_MEMORY);
      if (rl_ra_str->data)
	RL_FREE (rl_ra_str->data);
      rl_ra_str->data = NULL;
      rl_ra_str->size = rl_ra_str->alloc_size = 0;
      return (-1);
    }
  tail = rl_rarray_append (rl_ra_str, length);
  if (tail)
    strcat (--tail, str);
  else if (rl_ra_str->data)
    {
      RL_FREE (rl_ra_str->data);
      rl_ra_str->data = NULL;
      rl_ra_str->size = rl_ra_str->alloc_size = 0;
    }
  free (str);
  return (tail ? length : -1);
}

/**
 * Add pointer to list
 * @param ptrs resizable array with pointers on already save structures
 * @return Index of pointer in the list or -1 in case of memory operation error.
 * On higher level we need index because array is always reallocating and
 * pointer on element is changing (index remains constant).
 */
int
rl_add_ptr_to_list (rl_ra_rl_ptrdes_t * ptrs)
{
  rl_ptrdes_t * ptrdes = rl_rarray_append ((rl_rarray_t*)ptrs, sizeof (ptrs->ra.data[0]));
  if (NULL == ptrdes)
    return (-1);
  memset (ptrdes, 0, sizeof (*ptrdes));
  ptrdes->data = NULL;
  ptrdes->fd.type = NULL;
  ptrdes->fd.name = NULL;
  ptrdes->fd.hash_value = 0;
  ptrdes->fd.size = 0;
  ptrdes->fd.offset = 0;
  ptrdes->fd.rl_type = RL_TYPE_VOID;
  ptrdes->fd.rl_type_ext = RL_TYPE_EXT_NONE;
  ptrdes->fd.count = 0;
  ptrdes->fd.row_count = 0;
  ptrdes->fd.value = 0;
  ptrdes->fd.comment = NULL;
  ptrdes->fd.ext = NULL;
  ptrdes->level = 0;
  ptrdes->idx = -1; /* NB! To be initialized in depth search in rl_save */
  ptrdes->ref_idx = -1;
  ptrdes->parent = -1;
  ptrdes->first_child = -1;
  ptrdes->last_child = -1;
  ptrdes->prev = -1;
  ptrdes->next = -1;
  ptrdes->flags = RL_PDF_NONE;
  ptrdes->union_field_idx = 0;
  ptrdes->value = NULL;
  ptrdes->ext = NULL;
  return (ptrs->ra.size / sizeof (ptrs->ra.data[0]) - 1);
}

void
rl_add_child (int parent, int child, rl_ra_rl_ptrdes_t * ptrs)
{
  int last_child;

  if (child < 0)
    return;

  ptrs->ra.data[child].parent = parent;
  if (parent < 0)
    return;
  
  last_child = ptrs->ra.data[parent].last_child;
  if (last_child < 0)
    ptrs->ra.data[parent].first_child = child;
  else
    {
      ptrs->ra.data[last_child].next = child;
      ptrs->ra.data[child].prev = last_child;
      ptrs->ra.data[child].next = -1;
    }
  ptrs->ra.data[parent].last_child = child;
}

/**
 * Default hash function.
 * @param str a pointer on null terminated string
 * @return Hash function value.
 */
static uint64_t
hash_str (char * str)
{
  uint64_t hash_value = 0;
  if (NULL == str)
    return (hash_value);
  while (*str)
    hash_value = (hash_value + (unsigned char)*str++) * 0xFEDCBA987654321LL;
  return (hash_value);
}

/**
 * Function for checking if a number is prime.
 * @param x an integer number.
 * @return Returns TRUE if number is prime, FALSE otherwise. 
 */
static int
is_prime (int x)
{
  int i;
  if (1 == x)
    return (0);
  for (i = 2; i * i <= x; ++i)
    if (x % i == 0)
      return (0);
  return (!0);
}

/**
 * Type descriptors collection iterator.
 * @param func function for type descriptors processing
 * @param args auxiliary arguments
 * @return flag that cycle was not completed
 */
int
rl_td_foreach (int (*func) (rl_td_t*, void*), void * args)
{
  int count = rl_conf.des.size / sizeof (rl_conf.des.data[0]);
  int i;

  for (i = 0; i < count; ++i)
    if (func (rl_conf.des.data[i].tdp, args))
      return (!0);
  return (0);
}

#ifndef RL_TREE_LOOKUP
/**
 * Update hash with type descriptors
 * @param tdp root of linked list with type descriptors
 * @param hash rl_rarray_t with hash table for type descriptors
 * @return void
 */
static void
rl_update_td_hash (rl_td_t * tdp, rl_ra_rl_td_ptr_t * hash)
{
  int size;
  rl_td_ptr_t * x;

  tdp->hash_value = hash_str (tdp->type);

  if (NULL == hash->ra.data)
    hash->ra.alloc_size = hash->ra.size = 0;

  if (0 == hash->ra.size)
    {
      /* hash size is not defined. Let it be doubled number of elements in the list */
      size = 0;
      int td_count (rl_td_t * tdp, void * args) { ++size; return (0); }
      rl_td_foreach (td_count, NULL);
      hash->ra.size = 2 * size * sizeof (hash->ra.data[0]);
      if (hash->ra.data)
	RL_FREE (hash->ra.data);
      hash->ra.data = NULL;
    }
  else
    {
      /* Lets calculate hash bucket for new element */
      x = &hash->ra.data[tdp->hash_value % (hash->ra.size / sizeof (hash->ra.data[0]))];
      if (NULL == x->tdp)
	x->tdp = tdp; /* bucket was free and hash resize is not required */ 
      else
	{
	  if (x->tdp->hash_value == tdp->hash_value) /* hashes matched and non-collision hash table is not possible */
	    {
	      RL_MESSAGE (RL_LL_WARN, RL_MESSAGE_TYPES_HASHES_MATCHED, x->tdp->type, tdp->type);
	      return;
	    }
	  /* we have a collision, so we will need to find new hash size to avoid collisions */ 
	  RL_FREE (hash->ra.data);
	  hash->ra.data = NULL;
	  hash->ra.size = hash->ra.alloc_size = 0;
	}
    }

  while (NULL == hash->ra.data)
    {
      rl_td_ptr_t * array;
      int i;
      /* we need to find next prime number greater then hash->ra.size */ 
      size = ((hash->ra.size / sizeof (hash->ra.data[0])) | 1) + 2;
      while (!is_prime (size))
	size += 2;
      hash->ra.alloc_size = hash->ra.size = size * sizeof (hash->ra.data[0]);
      array = RL_MALLOC (hash->ra.alloc_size);
      /* check memory allocation */
      if (NULL == array)
	{
	  RL_MESSAGE (RL_LL_FATAL, RL_MESSAGE_OUT_OF_MEMORY);
	  hash->ra.alloc_size = hash->ra.size = 0;
	  return;
	}
      for (i = 0; i < size; ++i)
	array[i].tdp = NULL;
      /* populate list elements into hash table */
      int td_populate (rl_td_t * tdp, void * args)
      {
	rl_td_ptr_t * x = &array[tdp->hash_value % size];
	/* check for collision */
	if (x->tdp)
	  return (!0);
	x->tdp = tdp;
	return (0);
      }
      /* check that all elements were successfully populated into the hash table */
      if (rl_td_foreach (td_populate, NULL))
	RL_FREE (array); /* otherwise try to find new hash size */
      else
	hash->ra.data = array;
    }
}

#else /* RL_TREE_LOOKUP */

static int
cmp_tdp (const void * x, const void * y)
{
  return (strcmp (((const rl_td_t *) x)->type, ((const rl_td_t *) y)->type));
}

static void
rl_update_td_tree (rl_td_t * tdp, rl_red_black_tree_node_t ** tree)
{
  rl_td_t ** tdpp = tsearch (tdp, (void*)tree, cmp_tdp);
  if (NULL == tdpp)
    RL_MESSAGE (RL_LL_FATAL, RL_MESSAGE_OUT_OF_MEMORY);
}
#endif /* RL_TREE_LOOKUP */

/**
 * Type descriptor lookup function.
 * @param type stringified type name
 * @return pointer on type descriptor
 */
rl_td_t *
rl_get_td_by_name (char * type)
{
  rl_td_t * tdp;

  int td_cmp (rl_td_t * tdp_, void * args)
    {
      if (0 == strcmp (type, tdp_->type))
	{
	  *(rl_td_t**)args = tdp_;
	  return (!0);
	}
      return (0);
    }
  
#ifndef RL_TREE_LOOKUP
  if (rl_conf.hash.ra.data && rl_conf.hash.ra.size)
    {
      uint64_t hash_value = hash_str (type);
      tdp = rl_conf.hash.ra.data[hash_value % (rl_conf.hash.ra.size / sizeof (rl_conf.hash.ra.data[0]))].tdp;
      if (tdp && (hash_value == tdp->hash_value) && (0 == strcmp (type, tdp->type)))
	return (tdp);
      return (NULL);
    }
#else /* RL_TREE_LOOKUP */
  if (rl_conf.tree)
    {
      rl_td_t td = { .type = type };
      rl_td_t ** tdpp = tfind (&td, (void*)&rl_conf.tree, cmp_tdp);
      if (tdpp)
	return (*tdpp);
      else
	return (NULL);
    }
#endif /* RL_TREE_LOOKUP */
  if (rl_td_foreach (td_cmp, &tdp))
    return (tdp);
  return (NULL);
}

#define RL_TYPE_ANONYMOUS_UNION_TEMPLATE "rl_type_anonymous_union_%d_t"

static int
rl_anon_unions_extract (rl_td_t * tdp)
{
  int i, j;
  
  if (NULL == tdp)
    return (0);
  
  for (i = 0; i < tdp->fields.size / sizeof (tdp->fields.data[0]); ++i)
    if (RL_TYPE_ANON_UNION == tdp->fields.data[i].rl_type)
      {
	int count = tdp->fields.size / sizeof (tdp->fields.data[0]);
	for (j = i + 1; j < count; ++j)
	  if (RL_TYPE_END_ANON_UNION == tdp->fields.data[j].rl_type)
	    break;
	if (j >= count)
	  return (0);
	{
	  int fields_count = j - i; /* additional trailing element with rl_type = RL_TYPE_TRAILING_RECORD */
	  rl_fd_t * fields_data = RL_MALLOC (fields_count * sizeof (rl_fd_t));
	  static int rl_type_anonymous_union_cnt = 0;
	  char * type = RL_MALLOC (sizeof (RL_TYPE_ANONYMOUS_UNION_TEMPLATE) + RL_INT_TO_STRING_BUF_SIZE);
	  rl_td_t * tdp_ = RL_MALLOC (sizeof (rl_td_t));
	  int size = 0;
	  void ** ptr;

	  if ((NULL == fields_data) || (NULL == type) || (NULL == tdp_))
	    {
	      if (fields_data)
		RL_FREE (fields_data);
	      if (type)
		RL_FREE (type);
	      if (tdp_)
		RL_FREE (tdp_);
	      return (0);
	    }
	  ptr = rl_rarray_append ((void*)&rl_conf.allocated_mem, sizeof (void*));
	  if (ptr)
	    *ptr = fields_data;
	  ptr = rl_rarray_append ((void*)&rl_conf.allocated_mem, sizeof (void*));
	  if (ptr)
	    *ptr = type;
	  ptr = rl_rarray_append ((void*)&rl_conf.allocated_mem, sizeof (void*));
	  if (ptr)
	    *ptr = tdp_;
	  
	  sprintf (type, RL_TYPE_ANONYMOUS_UNION_TEMPLATE, rl_type_anonymous_union_cnt++);
	  for (j = 0; j < fields_count - 1; ++j)
	    {
	      fields_data[j] = tdp->fields.data[i + 1 + j];
	      if (fields_data[j].size > size)
		size = fields_data[j].size;
	      fields_data[j].offset = 0;
	    }
	  fields_data[fields_count - 1].rl_type = RL_TYPE_TRAILING_RECORD; /* trailing record */
	  tdp_->rl_type = RL_TYPE_ANON_UNION;
	  tdp_->type = type;
	  tdp_->attr = tdp->fields.data[i].comment; /* anonymous union stringified attributes are saved into comments field */
	  tdp_->ext = NULL;
	  tdp_->size = size;
	  tdp_->fields.data = fields_data;

	  tdp->fields.data[i].comment = tdp->fields.data[i + fields_count].comment; /* copy comment from RL_END_ANON_UNION record */
	  for (j = i + fields_count + 1; j < count; ++j)
	    tdp->fields.data[j - fields_count] = tdp->fields.data[j];
	  tdp->fields.size -= fields_count * sizeof (tdp->fields.data[0]);
	  tdp->fields.data[i].type = type;
	  tdp->fields.data[i].rl_type = RL_TYPE_ANON_UNION;
	  tdp->fields.data[i].size = size;

	  if (rl_add_type (tdp_, tdp->fields.data[i].comment, NULL))
	    {
	      RL_MESSAGE (RL_LL_ERROR, RL_MESSAGE_ANON_UNION_TYPE_ERROR, type);
	      return (0);
	    }
	}
      }
  return (!0);
}

static int
cmp_enums_by_value (const void * x, const void * y)
{
  return ((((const rl_fd_t *) x)->value > ((const rl_fd_t *) y)->value) - (((const rl_fd_t *) x)->value < ((const rl_fd_t *) y)->value));
}

static int
cmp_enums_by_name (const void * x, const void * y)
{
  return (strcmp (((const rl_fd_t *) x)->name, ((const rl_fd_t *) y)->name));
}

static int
rl_add_enum (rl_td_t * tdp)
{
  int count = tdp->fields.size / sizeof (tdp->fields.data[0]);
  int i;
  tdp->lookup_by_value = NULL;
  for (i = 0; i < count; ++i)
    {
      rl_fd_t ** fdpp = tsearch (&tdp->fields.data[i], (void*)&rl_conf.enum_by_name, cmp_enums_by_name);  
      if (NULL == fdpp)
	{
	  RL_MESSAGE (RL_LL_FATAL, RL_MESSAGE_OUT_OF_MEMORY);
	  return (-1);
	}
      if (*fdpp != &tdp->fields.data[i])
	{
	  RL_MESSAGE (RL_LL_WARN, RL_MESSAGE_DUPLICATED_ENUMS, (*fdpp)->name, tdp->type);
	  return (-1);
	}
      fdpp = tsearch (&tdp->fields.data[i], (void*)&tdp->lookup_by_value, cmp_enums_by_value);  
      if (NULL == fdpp)
	{
	  RL_MESSAGE (RL_LL_FATAL, RL_MESSAGE_OUT_OF_MEMORY);
	  return (-1);
	}
    }
  return (0);
}

rl_fd_t *
rl_get_enum_by_value (rl_td_t * tdp, int64_t value)
{
  rl_fd_t fd = { .value = value };
  rl_fd_t ** fdpp = tfind (&fd, (void*)&tdp->lookup_by_value, cmp_enums_by_value);
  if (fdpp)
    return (*fdpp);
  return (NULL);
}

int
rl_get_enum_by_name (int64_t * value, char * name)
{
  rl_fd_t fd = { .name = name };
  rl_fd_t ** fdpp = tfind (&fd, (void*)&rl_conf.enum_by_name, cmp_enums_by_name);
  if (fdpp)
    *value = (*fdpp)->value;
  return (!!fdpp);
}

static int
rl_check_fields_names (rl_td_t * tdp)
{
  int i, j;
  int count = tdp->fields.size / sizeof (tdp->fields.data[0]);
  for (i = 0; i < count; ++i)
    {
      /* Check names for of the fileds. Array definitions may contain brackets which are not acceptable as XML tag names. */
      char * name = tdp->fields.data[i].name;
      if (name)
	{
	  for (; isalnum (*name) || (*name == '_'); ++name); /* skip valid characters */
	  if (*name) /* check for invalid characters */
	    {
	      /* all internally allocated memory should be added to special array for proper cleanup */
	      void ** ptr = rl_rarray_append ((void*)&rl_conf.allocated_mem, sizeof (rl_conf.allocated_mem.data[0]));
	      tdp->fields.data[i].name = strndup (tdp->fields.data[i].name, name - tdp->fields.data[i].name);
	      if (ptr)
		*ptr = tdp->fields.data[i].name;
	    }
	}
    }
  /* check for name duplicates */
  for (i = 0; i < count; ++i)
    for (j = i + 1; j < count; ++j)
      if (tdp->fields.data[i].name && tdp->fields.data[j].name && (0 == strcmp (tdp->fields.data[i].name, tdp->fields.data[j].name)))
	RL_MESSAGE (RL_LL_WARN, RL_MESSAGE_DUPLICATED_FIELDS, tdp->fields.data[i].name, tdp->type);
  
  return (0);
}

static int
rl_build_field_names_hash (rl_td_t * tdp)
{
  int i, j;
  int fields_count = tdp->fields.size / sizeof (tdp->fields.data[0]);

  for (i = 0; i < fields_count; ++i)
    tdp->fields.data[i].hash_value = hash_str (tdp->fields.data[i].name);

  tdp->lookup_by_name.size = tdp->lookup_by_name.alloc_size = 0;
  tdp->lookup_by_name.data = NULL;
  for (i = 0; i < fields_count; ++i)
    for (j = i + 1; j < fields_count; ++j)
      if (tdp->fields.data[i].hash_value == tdp->fields.data[j].hash_value)
	return (!0);
  
  while (NULL == tdp->lookup_by_name.data)
    {
      rl_fd_ptr_t * array;
      int size = ((tdp->lookup_by_name.size / sizeof (tdp->lookup_by_name.data[0])) | 1) + 2;
      /* we need to find next prime number greater then current hash table size */ 
      while (!is_prime (size))
	size += 2;
      tdp->lookup_by_name.alloc_size = tdp->lookup_by_name.size = size * sizeof (tdp->lookup_by_name.data[0]);
      array = RL_MALLOC (tdp->lookup_by_name.alloc_size);
      /* check memory allocation */
      if (NULL == array)
	{
	  RL_MESSAGE (RL_LL_FATAL, RL_MESSAGE_OUT_OF_MEMORY);
	  tdp->lookup_by_name.alloc_size = tdp->lookup_by_name.size = 0;
	  return (!0);
	}
      for (i = 0; i < size; ++i)
	array[i].fdp = NULL;
      /* populate list elements into hash table */
      for (i = 0; i < fields_count; ++i)
	{
	  rl_fd_ptr_t * x = &array[tdp->fields.data[i].hash_value % size];
	  /* check for collision */
	  if (x->fdp)
	    break;
	  else
	    x->fdp = &tdp->fields.data[i];
	}
      /* check that all elements were successfully populated into the hash table */
      if (i >= fields_count)
	tdp->lookup_by_name.data = array;
      else
	RL_FREE (array); /* otherwise try to find new hash size */
    }
  return (0);
}

static int
rl_check_fields_types (rl_td_t * tdp, void * args)
{
  int i;
  rl_td_t * tdp_;
  int fields_count = tdp->fields.size / sizeof (tdp->fields.data[0]);
  
  for (i = 0; i < fields_count; ++i)
    switch (tdp->fields.data[i].rl_type)
      {
	/* RL_AUTO type resolution */
      case RL_TYPE_NONE:
	/* Enum detection */
      case RL_TYPE_INT8:
      case RL_TYPE_UINT8:
      case RL_TYPE_INT16:
      case RL_TYPE_UINT16:
      case RL_TYPE_INT32:
      case RL_TYPE_UINT32:
      case RL_TYPE_INT64:
      case RL_TYPE_UINT64:
	tdp_ = rl_get_td_by_name (tdp->fields.data[i].type);
	if (tdp_)
	  tdp->fields.data[i].rl_type = tdp_->rl_type;
	break;
      default:
	break;
      }
  return (0);
}

rl_fd_t *
rl_get_fd_by_name (rl_td_t * tdp, char * name)
{
  if (tdp->lookup_by_name.data)
    {
      uint64_t hash_value = hash_str (name);
      rl_fd_t * fdp = tdp->lookup_by_name.data[hash_value % (tdp->lookup_by_name.size / sizeof (tdp->lookup_by_name.data[0]))].fdp;
      if (fdp && (hash_value == fdp->hash_value) && (0 == strcmp (name, fdp->name)))
	return (fdp);
    }
  else
    {
      int i;
      int fields_count = tdp->fields.size / sizeof (tdp->fields.data[0]);
      for (i = 0; i < fields_count; ++i)
	if (0 == strcmp (name, tdp->fields.data[i].name))
	  return (&tdp->fields.data[i]);
    }
  return (NULL);
}

/**
 * Add type description into repository
 * @param tdp a pointer on statically initialized type descriptor
 * @param comment comments
 * @param ext auxiliary void pointer
 * @return status, 0 - new type was added, !0 - type was already registered
 */
int __attribute__ ((sentinel(0)))
rl_add_type (rl_td_t * tdp, char * comment, ...)
{
  va_list args;
  void * ext;
  int count;

  if (NULL == tdp)
    return (0); /* assert */
  /* check whether this type is already in the list */
  if (rl_get_td_by_name (tdp->type))
    return (!0); /* this type is already registered */

  va_start (args, comment);
  ext = va_arg (args, void*);
  va_end (args);

  for (count = 0; RL_TYPE_TRAILING_RECORD != tdp->fields.data[count].rl_type; ++count);
  tdp->fields.size = count * sizeof (tdp->fields.data[0]);
  tdp->fields.ext = NULL;
  
  if ((NULL != comment) && comment[0])
    tdp->comment = comment;

  if (NULL != ext)
    tdp->ext = ext;

  rl_check_fields_names (tdp);
  rl_anon_unions_extract (tdp);
  rl_build_field_names_hash (tdp);
  
  /* NB! not thread safe - only calls from __constructor__ assumed */
  {
    rl_td_t ** tdpp = rl_rarray_append ((void*)&rl_conf.des, sizeof (rl_conf.des.data[0]));
    if (NULL == tdpp)
      {
	RL_MESSAGE (RL_LL_FATAL, RL_MESSAGE_OUT_OF_MEMORY);
	return (!0);
      }
    *tdpp = tdp;
  }
#ifndef RL_TREE_LOOKUP
  rl_update_td_hash (tdp, &rl_conf.hash);
#else /* RL_TREE_LOOKUP */
  rl_update_td_tree (tdp, &rl_conf.tree);
#endif /*  RL_TREE_LOOKUP */
  tdp->lookup_by_value = NULL; /* should be in rl_add_enum, but produces warning for non-enum types due to uninitialized memory */
  if (RL_TYPE_ENUM == tdp->rl_type)
    rl_add_enum (tdp);

  rl_td_foreach (rl_check_fields_types, tdp);
  return (0);
}

int
rl_parse_add_node (rl_load_t * rl_load)
{
  int idx = rl_add_ptr_to_list ((rl_ra_rl_ptrdes_t*)rl_load->ptrs);
  if (idx < 0)
    return (idx);
  rl_add_child (rl_load->parent, idx, (rl_ra_rl_ptrdes_t*)rl_load->ptrs);
  return (idx);
}

/**
 * Read into newly allocated string one xml object.
 * @param fd file descriptor
 * @return Newly allocated string with xml or NULL in case of any errors
 */
char *
rl_read_xml_doc (FILE * fd)
{
  int size, max_size;
  char * str;
  char c_ = 0;
  int opened_tags = 0;
  int tags_to_close = -1;
  int count = 2;

  max_size = 2 * 1024; /* initial string size */
  str = (char*)RL_MALLOC (max_size);
  if (NULL == str)
    {
      RL_MESSAGE (RL_LL_FATAL, RL_MESSAGE_OUT_OF_MEMORY);
      return (NULL);
    }
  size = -1;
  
  for (;;)
    {
      int c = fgetc (fd);
      if ((c == EOF) || (c == 0))
	{
	  RL_MESSAGE (RL_LL_ERROR, RL_MESSAGE_UNEXPECTED_END);
	  RL_FREE (str);
	  return (NULL);
	}
      
      str[++size] = c;
      if (size == max_size - 1)
	{
	  void * str_;
	  max_size <<= 1; /* double input bufer size */
	  str_ = RL_REALLOC (str, max_size);
	  if (NULL == str_)
	    {
	      RL_MESSAGE (RL_LL_FATAL, RL_MESSAGE_OUT_OF_MEMORY);
	      RL_FREE (str);
	      return (NULL);
	    }
	  str = (char*) str_;
	}

      if ((0 == opened_tags) && !(('<' == c) || isspace (c)))
	{
	  RL_MESSAGE (RL_LL_ERROR, RL_MESSAGE_UNEXPECTED_DATA);
	  RL_FREE (str);
	  return (NULL);
	}

      if ('<' == c)
	opened_tags += 2;
      if (('/' == c_) && ('>' == c))
	tags_to_close = -2;
      if (('?' == c_) && ('>' == c))
	tags_to_close = -2;
      if (('<' == c_) && ('/' == c))
	tags_to_close = -3;
      if ('>' == c)
	{
	  opened_tags += tags_to_close;
	  tags_to_close = -1;
	  if (opened_tags < 0)
	    {
	      RL_MESSAGE (RL_LL_ERROR, RL_MESSAGE_UNBALANCED_TAGS);
	      RL_FREE (str);
	      return (NULL);
	    }
	  if (0 == opened_tags)
	    if (0 == --count)
	      {
		str[++size] = 0;
		return (str);
	      }
	}
      if (!isspace (c))
	c_ = c;
    }
}
