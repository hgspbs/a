#include <linux/module.h>
#include <linux/rtc.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/random.h>

struct a_priv {
	struct rtc_device *rtc;
	spinlock_t lock;
	time64_t offset;
	int mode;
	uint8_t buffer[16];
};
static struct platform_device *a_dev;

static int a_read_time(struct device *dev, struct rtc_time *tm)
{
	struct a_priv *priv = dev_get_drvdata(dev);
	unsigned long flags;
	int m;
	time64_t t;

	spin_lock_irqsave(&priv->lock, flags);
	m = priv->mode;
	spin_unlock_irqrestore(&priv->lock, flags);
	if ((m > 0 && m <= 100) || (m < 0 && m > -100)) {
		t = ktime_get_real_seconds() + priv->offset;
		t += (t * m) / 100;
	} else {
		t = 0;
		get_random_bytes(&t, 5);
	}
	rtc_time64_to_tm(t, tm);
	return 0;
}

static int a_set_time(struct device *dev, struct rtc_time *tm)
{
	struct a_priv *priv = dev_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);
	if ((priv->mode > 0 && priv->mode <= 100) || (priv->mode < 0 && priv->mode > -100)) {
		priv->offset = (rtc_tm_to_time64(tm) * 100) / (100 + priv->mode) - ktime_get_real_seconds();
	} else {
		priv->offset = 0;
	}
	spin_unlock_irqrestore(&priv->lock, flags);
	return 0;
}

static int a_seq_show(struct seq_file *seq, void *offset)
{
	struct a_priv *priv = seq->private;

	seq_printf(seq,"%i\n", priv->mode);
	return 0;
}

static ssize_t a_proc_write(struct file *file, const char __user * buffer, size_t count, loff_t * ppos)
{
	struct a_priv *priv = PDE_DATA(file_inode(file));
	unsigned long flags;
	int v;

	if (!priv || count >= sizeof (priv->buffer))
		return -EINVAL;
	if (copy_from_user(priv->buffer, buffer, count)) {
		return -EFAULT;
	}
	priv->buffer[count] = 0;
	if (kstrtoint(priv->buffer, 0, &v)) {
		return -EINVAL;
	}
	spin_lock_irqsave(&priv->lock, flags);
	priv->mode = v;
	spin_unlock_irqrestore(&priv->lock, flags);
	return count;
}

static int a_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, a_seq_show, PDE_DATA(inode));
}

static const struct rtc_class_ops a_ops = {
	.read_time = a_read_time,
	.set_time = a_set_time,
};

static const struct proc_ops a_proc_ops = {
	.proc_open = a_proc_open,
	.proc_read = seq_read,
	.proc_write = a_proc_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int a_probe(struct platform_device *pdev)
{
	struct rtc_device *rtc;
	struct a_priv *priv;

	priv = devm_kzalloc(&pdev->dev, sizeof(struct a_priv), GFP_KERNEL);
	if (!priv)
	    return -ENOMEM;
	spin_lock_init(&priv->lock);
	platform_set_drvdata(pdev, priv);
	rtc = devm_rtc_device_register(&pdev->dev, "a", &a_ops, THIS_MODULE);
	if (IS_ERR(rtc))
		return PTR_ERR(rtc);
	priv->rtc = rtc;
	if (!proc_create_data("a", 0, NULL, &a_proc_ops, priv))
		return -ENODEV;
	return 0;
}

static int a_remove(struct platform_device *pdev)
{
	struct a_priv *priv = dev_get_drvdata(&pdev->dev);

	remove_proc_entry("a", NULL);
	return 0;
}

static struct platform_driver a_driver = {
	.driver = {
		.name = "a",
	},
	.probe = a_probe,
	.remove = a_remove
};

static int __init a_init(void)
{
	int err;

	err = platform_driver_register(&a_driver);
	if (err)
		return err;
	a_dev = platform_device_alloc("a", 0);
	if (a_dev) {
		err = platform_device_add(a_dev);
	} else {
		err = -ENOMEM;
	}
	if (!a_dev || err) {
		if (a_dev) {
			platform_device_del(a_dev);
			platform_device_put(a_dev);
		}
		platform_driver_unregister(&a_driver);
	}
	return err;
}
module_init(a_init);

static void __exit a_exit(void)
{
	platform_device_unregister(a_dev);
	platform_driver_unregister(&a_driver);
}
module_exit(a_exit);

MODULE_AUTHOR("Andrey Burdovitsyn");
MODULE_DESCRIPTION("a");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:a");
