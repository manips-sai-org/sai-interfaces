import redis 
import sys
import numpy as np

if len(sys.argv) != 2:
    print('usage: python3 {} <# of joints>'.format(sys.argv[0]))
    exit(0)

try:
    num_joints = int(sys.argv[1])
except:
    print('usage: python3 {} <# of joints>'.format(sys.argv[0]))
    exit(0)

# mode key
MODE_KEY = 'sai2::interfaces::tutorial::mode'

# joint task keys
JOINT_KEY = 'sai2::interfaces::tutorial::q'
KP_GAIN_KEY = 'sai2::interfaces::tutorial::joint_kp'
KV_GAIN_KEY = 'sai2::interfaces::tutorial::joint_kv'

# posori task keys
EE_POS_KEY = 'sai2::interfaces::tutorial::ee_pos'
EE_ORI_KEY = 'sai2::interfaces::tutorial::ee_ori'
EE_POS_KP_KEY = 'sai2::interfaces::tutorial::ee_pos_kp'
EE_POS_KV_KEY = 'sai2::interfaces::tutorial::ee_pos_kv'
EE_ORI_KP_KEY = 'sai2::interfaces::tutorial::ee_ori_kp'
EE_ORI_KV_KEY = 'sai2::interfaces::tutorial::ee_ori_kv'

initial_joint_values = 2 * np.pi * np.random.rand(num_joints)
initial_pos_values = 3 * np.random.rand(3)
initial_ori_values = 2 * np.pi * np.random.rand(3)

r = redis.Redis()

# mode
r.set(MODE_KEY, 'joint')

# joint task
r.set(JOINT_KEY, str(initial_joint_values.tolist()))
r.set(KP_GAIN_KEY, '100')
r.set(KV_GAIN_KEY, '2')

# posori task
r.set(EE_POS_KEY, str(initial_pos_values.tolist()))
r.set(EE_ORI_KEY, str(initial_ori_values.tolist()))
r.set(EE_POS_KP_KEY, '100')
r.set(EE_POS_KV_KEY, '2')
r.set(EE_ORI_KP_KEY, '75')
r.set(EE_ORI_KV_KEY, '3')

# print joint
print('{} set to {}'.format(JOINT_KEY, r.get(JOINT_KEY)))
print('{} set to {}'.format(KP_GAIN_KEY, r.get(KP_GAIN_KEY)))
print('{} set to {}'.format(KV_GAIN_KEY, r.get(KV_GAIN_KEY)))

# print posori
print('{} set to {}'.format(EE_POS_KEY, r.get(EE_POS_KEY)))
print('{} set to {}'.format(EE_ORI_KEY, r.get(EE_ORI_KEY)))
print('{} set to {}'.format(EE_POS_KP_KEY, r.get(EE_POS_KP_KEY)))
print('{} set to {}'.format(EE_POS_KV_KEY, r.get(EE_POS_KV_KEY)))
print('{} set to {}'.format(EE_ORI_KP_KEY, r.get(EE_ORI_KP_KEY)))
print('{} set to {}'.format(EE_ORI_KV_KEY, r.get(EE_ORI_KV_KEY)))
