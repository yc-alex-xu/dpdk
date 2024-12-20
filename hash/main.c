#include <rte_errno.h>
#include <rte_hash.h>
#include <rte_hash_crc.h>
#include <rte_jhash.h>
#include <rte_malloc.h>
#include <stdio.h>
#include <stdint.h>

/*
map: key ---> idx of the array
*/
int hash_map_test(int num_item)
{
  int hash_size = 1 << 10;
  printf("-------%s array size %d entries of hash %d-------\n", __func__, num_item, hash_size);
  const struct rte_hash_parameters stHashParam = {
      .name = "hash_test",
      .entries = hash_size,
      .reserved = 0,
      .key_len = sizeof(int),
      .hash_func = rte_jhash,
      .hash_func_init_val = 0,
      .socket_id = rte_socket_id(),
      .extra_flag = 0,
  };

  struct rte_hash *hash_table = rte_hash_create(&stHashParam);
  if (!hash_table)
  {
    rte_panic("Failed to create hash table, errno = %d\n", rte_errno);
    return -1;
  }

  int *arr = (int *)rte_malloc(NULL, sizeof(int32_t) * num_item, 0);
  if (arr == NULL)
    return -1;

  for (int key = 0; key < num_item; ++key)
  {
    int idx = key;
    int ret = rte_hash_add_key_data(hash_table, &key, &idx);
    if (ret == 0)
    {
      arr[idx] = key + 1;
    }
    else if (ret == -ENOSPC)
    {
      rte_panic("no space when add key %d data %d\n", key, idx);
    }
    else if (ret == -EINVAL)
    {
      rte_panic("invalid  when add key %d data %d\n", key, idx);
    }
    else
    {
      printf(" ret %d  when add key %d data %d\n", ret, key, idx);
    }
  }

  for (int key = 0; key < num_item; ++key)
  {
    int ret = rte_hash_lookup(hash_table, &key);
    if (ret >= 0)
    {
      if (arr[ret] != key + 1)
      {
        rte_panic("wrong: key %d data %d\n", key, arr[ret]);
      }
    }
    else if (ret == -ENOENT)
    {
      printf("key %d is not found\n", key);
    }
    else if (ret == -EINVAL)
    {
      rte_panic("key %d  is invalid\n", key);
    }
    else
    {
      printf("ret %d when retrieve  key %d \n", ret, key);
    }
  }

  rte_free(arr);
  rte_hash_free(hash_table);
  return 0;
}

int main(int argc, char **argv)
{
  int ret = rte_eal_init(argc, argv);
  if (ret < 0)
    rte_exit(EXIT_FAILURE, "rte_eal_init()\n");
  hash_map_test(100);
  hash_map_test(512);
  return 0;
}
